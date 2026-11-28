#pragma once

#include "../System.h"
#include "../volume/AABB.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Atlas {

    template<class T>
    void try_get_json(const json& j, const char* name, T& t) {

        auto it = j.find(name);
        if (it != j.end()) {
            it->get_to(t);
        }

    }

    template<class T>
    void try_get_json(const json& j, const char* name, T& t, const T def) {

        auto it = j.find(name);
        if (it != j.end()) {
            it->get_to(t);
        }
        else {
            t = def;
        }

    }

    template<class T>
    void try_get_binary_json(const json& j, const char* name, std::vector<T>& data, bool binary = true) {

        auto it = j.find(name);
        if (it == j.end())
            return;

        auto copy = [&](const std::vector<uint8_t>& binaryData) {
            AE_ASSERT(binaryData.size() % sizeof(T) == 0 && "Binary data isn't a multiple of the element size");
            if (!binaryData.empty()) {
                data.resize(binaryData.size() / sizeof(T));
                std::memcpy(data.data(), binaryData.data(), data.size() * sizeof(T));
            }
        };

        // The binary value is read in place, large buffers aren't copied twice
        if (binary) {
            copy(it->get_binary());
        }
        else {
            std::vector<uint8_t> binaryData;
            it->get_to(binaryData);
            copy(binaryData);
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
            j[name] = json::binary_t(std::move(binaryData));
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