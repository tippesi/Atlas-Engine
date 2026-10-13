#include "Common.h"

#include "loader/AssetLoader.h"

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

    bool MatchPattern(const std::string& pattern, const std::string& text) {

        // Iterative wildcard matching with backtracking to the last star
        size_t p = 0, t = 0, starIdx = std::string::npos, matchIdx = 0;
        while (t < text.size()) {
            if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == text[t])) {
                p++;
                t++;
            }
            else if (p < pattern.size() && pattern[p] == '*') {
                starIdx = p++;
                matchIdx = t;
            }
            else if (starIdx != std::string::npos) {
                p = starIdx + 1;
                t = ++matchIdx;
            }
            else {
                return false;
            }
        }

        while (p < pattern.size() && pattern[p] == '*')
            p++;

        return p == pattern.size();

    }

    bool MatchAnyPattern(const std::vector<std::string>& patterns, const std::string& text) {

        for (const auto& pattern : patterns)
            if (MatchPattern(pattern, text))
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

    json FromVec3(vec3 v) {

        // Round a bit to keep the output readable
        auto round = [](float x) {

            return std::round(x * 10000.0f) / 10000.0f;

        };
        return json::array({ round(v.x), round(v.y), round(v.z) });

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

}
