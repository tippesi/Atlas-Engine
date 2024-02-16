#pragma once

#include "../System.h"
#include "../volume/AABB.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace glm {

    void to_json(json& j, const ivec2& p);

    void from_json(const json& j, ivec2& p);

    void to_json(json& j, const ivec3& p);

    void from_json(const json& j, ivec3& p);

    void to_json(json& j, const ivec4& p);

    void from_json(const json& j, ivec4& p);

    void to_json(json& j, const vec2& p);

    void from_json(const json& j, vec2& p);

    void to_json(json& j, const vec3& p);

    void from_json(const json& j, vec3& p);

    void to_json(json& j, const vec4& p);

    void from_json(const json& j, vec4& p);

    void to_json(json& j, const quat& p);

    void from_json(const json& j, quat& p);

    void to_json(json& j, const mat3& p);

    void from_json(const json& j, mat3& p);

    void to_json(json& j, const mat4& p);

    void from_json(const json& j, mat4& p);

}

namespace Atlas::Volume {

    void to_json(json& j, const Volume::AABB& p);

    void from_json(const json& j, Volume::AABB& p);

}