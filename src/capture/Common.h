#pragma once

#include "System.h"
#include "common/SerializationHelper.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Atlas::Capture {

    namespace fs = std::filesystem;

    using glm::u8vec4;
    using glm::uvec4;

    // Directory the tool was launched from. The engine main changes the working directory
    // to the executable directory, so this is captured during static initialization
    const fs::path& GetLaunchDirectory();

    // Absolute path of the engine's filesystem data directory
    fs::path GetAssetDirectory();

    // Resolves a user supplied path relative to the launch directory
    fs::path ResolveUserPath(const std::string& path);

    // Converts a user supplied scene path into a path relative to the asset directory,
    // e.g. "data/scenes/Kingdom.aescene" or "scenes/Kingdom.aescene" -> "scenes/Kingdom.aescene"
    std::string ToAssetPath(const std::string& path);

    // Case sensitive glob match supporting '*' and '?'
    bool MatchPattern(const std::string& pattern, const std::string& text);

    bool MatchAnyPattern(const std::vector<std::string>& patterns, const std::string& text);

    vec3 ToVec3(const json& j, vec3 defaultValue = vec3(0.0f));

    // Horizontal position from [x, z] or [x, y, z]
    vec2 ToVec2(const json& j);

    json FromVec3(vec3 v);

    // Rounds to a number of decimals to keep the JSON output readable
    double Round(double value, int32_t decimals = 3);

    // Unit vector for angles in degrees, azimuth 0 points towards +Z and 90 towards +X
    vec3 GetDirection(float azimuth, float elevation);

    std::string FormatFloat(float value, int32_t precision = 3);

    std::string FormatVec3(vec3 v, int32_t precision = 3);

    std::string SanitizeFileName(const std::string& name);

}
