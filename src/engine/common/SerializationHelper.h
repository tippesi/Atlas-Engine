#pragma once

#include "../System.h"
#include "../volume/AABB.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Atlas {

    template<class T>
    void try_get_json(const json& j, const char* name, T& t) {

        if (j.contains(name)) {
            j.at(name).get_to(t);
        }

    }

    template<class T>
    void try_get_json(const json& j, const char* name, T& t, const T def) {

        if (j.contains(name)) {
            j.at(name).get_to(t);
        }
        else {
            t = def;
        }

    }

    template<class T>
    void try_get_binary_json(const json& j, const char* name, std::vector<T>& data, bool binary = true) {

        std::vector<uint8_t> binaryData;
        if (binary)
            binaryData = j[name].get_binary();
        else
            j.at(name).get_to(binaryData);
        if (!binaryData.empty()) {
            data.resize(binaryData.size() / sizeof(T));
            std::memcpy(data.data(), binaryData.data(), binaryData.size());
        }

    }

    template<class T>
    void set_binary_json(json& j, const char* name, const std::vector<T>& data, bool binary = true) {

        std::vector<uint8_t> binaryData;
        if (!data.empty()) {
            binaryData.resize(data.size() * sizeof(T));
            std::memcpy(binaryData.data(), data.data(), binaryData.size());
        }

        if (binary) {
            j[name] = json::binary_t(binaryData);
        }
        else {
            j[name] = binaryData;
        }

    }

}

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