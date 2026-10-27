#include "Common.h"

#include "loader/AssetLoader.h"

#include <fstream>
#include <sstream>
#include <iomanip>

namespace Atlas::Capture {

    // Runs before main(), which changes the working directory to the executable directory
    static const fs::path launchDirectory = fs::current_path();

    const fs::path& GetLaunchDirectory() {

        return launchDirectory;

    }

    fs::path GetAssetDirectory() {

        return fs::weakly_canonical(Loader::AssetLoader::GetDataDirectory());

    }

    fs::path ResolveUserPath(const std::string& path) {

        fs::path userPath(path);
        if (userPath.is_absolute())
            return userPath.lexically_normal();

        return (launchDirectory / userPath).lexically_normal();

    }

    std::string ToAssetPath(const std::string& path) {

        auto assetDirectory = GetAssetDirectory();

        // Prefer paths that exist relative to the launch directory (e.g. data/scenes/x.aescene)
        auto userPath = ResolveUserPath(path);
        if (fs::exists(userPath)) {
            auto relative = fs::weakly_canonical(userPath).lexically_relative(assetDirectory);
            if (!relative.empty() && relative.native()[0] != '.')
                return relative.generic_string();
            // Outside of the asset directory, the engine accepts absolute paths
            return fs::weakly_canonical(userPath).generic_string();
        }

        auto generic = fs::path(path).generic_string();
        if (generic.starts_with("data/"))
            generic = generic.substr(5);

        return generic;

    }

    bool MatchAnyPattern(const std::vector<std::string>& patterns, const std::string& text) {

        for (const auto& pattern : patterns)
            if (Helper::String::MatchWildcard(pattern, text))
                return true;

        return false;

    }

    vec3 ToVec3(const json& j, vec3 defaultValue) {

        if (j.is_array() && j.size() == 3)
            return vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
        if (j.is_number())
            return vec3(j.get<float>());

        return defaultValue;

    }

    vec2 ToVec2(const json& j) {

        return vec2(j.at(0).get<float>(), j.at(j.size() == 3 ? 2 : 1).get<float>());

    }

    json FromVec3(vec3 v) {

        return json::array({ Round(v.x, 4), Round(v.y, 4), Round(v.z, 4) });

    }

    double Round(double value, int32_t decimals) {

        auto scale = std::pow(10.0, double(decimals));
        return std::round(value * scale) / scale;

    }

    vec3 GetDirection(float azimuth, float elevation) {

        auto az = glm::radians(azimuth), el = glm::radians(elevation);
        return vec3(std::cos(el) * std::sin(az), std::sin(el), std::cos(el) * std::cos(az));

    }

    std::string FormatFloat(float value, int32_t precision) {

        std::ostringstream stream;
        stream << std::fixed << std::setprecision(precision) << value;
        return stream.str();

    }

    std::string FormatVec3(vec3 v, int32_t precision) {

        return "(" + FormatFloat(v.x, precision) + ", " + FormatFloat(v.y, precision) + ", " +
            FormatFloat(v.z, precision) + ")";

    }

    std::string FormatTime(float time) {

        return "t=" + FormatFloat(time, 2) + "s";

    }

    std::string SanitizeFileName(const std::string& name) {

        std::string result;
        for (auto c : name) {
            if (std::isalnum(uint8_t(c)) || c == '-' || c == '_' || c == '.')
                result.push_back(c);
            else
                result.push_back('_');
        }

        return result.empty() ? "unnamed" : result;

    }

    bool WriteFileAtomic(const fs::path& path, const std::string& content) {

        auto temporary = path;
        temporary += ".tmp";
        {
            std::ofstream stream(temporary, std::ios::binary);
            if (!stream.is_open())
                return false;
            stream << content;
        }

        std::error_code error;
        fs::rename(temporary, path, error);
        return !error;

    }

}
