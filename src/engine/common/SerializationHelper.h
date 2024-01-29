#ifndef AE_SERIALIZERHELPER_H
#define AE_SERIALIZERHELPER_H

#include "../System.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Atlas {

    void to_json(json& j, const vec2& p);

    void from_json(const json& j, vec2& p);

    void to_json(json& j, const vec3& p);

    void from_json(const json& j, vec3& p);

    void to_json(json& j, const vec4& p);

    void from_json(const json& j, vec4& p);

    void to_json(json& j, const mat3& p);

    void from_json(const json& j, mat3& p);

    void to_json(json& j, const mat4& p);

    void from_json(const json& j, mat4& p);

}

#endif