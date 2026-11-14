#include "SerializationHelper.h"

namespace glm {

    void to_json(json &j, const ivec2 &p) {
        j = json{{"x", p.x},
                 {"y", p.y}};
    }

    void from_json(const json &j, ivec2 &p) {
        j.at("x").get_to(p.x);
        j.at("y").get_to(p.y);
    }

    void to_json(json &j, const ivec3 &p) {
        j = json{{"x", p.x},
                 {"y", p.y},
                 {"z", p.z}};
    }

    void from_json(const json &j, ivec3 &p) {
        j.at("x").get_to(p.x);
        j.at("y").get_to(p.y);
        j.at("z").get_to(p.z);
    }

    void to_json(json &j, const ivec4 &p) {
        j = json{{"x", p.x},
                 {"y", p.y},
                 {"z", p.z},
                 {"w", p.w}};
    }

    void from_json(const json &j, ivec4 &p) {
        j.at("x").get_to(p.x);
        j.at("y").get_to(p.y);
        j.at("z").get_to(p.z);
        j.at("w").get_to(p.w);
    }

    void to_json(json &j, const vec2 &p) {
        j = json{{"x", p.x},
                 {"y", p.y}};
    }

    void from_json(const json &j, vec2 &p) {
        j.at("x").get_to(p.x);
        j.at("y").get_to(p.y);
    }

    void to_json(json &j, const vec3 &p) {
        j = json{{"x", p.x},
                 {"y", p.y},
                 {"z", p.z}};
    }

    void from_json(const json &j, vec3 &p) {
        j.at("x").get_to(p.x);
        j.at("y").get_to(p.y);
        j.at("z").get_to(p.z);
    }

    void to_json(json &j, const vec4 &p) {
        j = json{{"x", p.x},
                 {"y", p.y},
                 {"z", p.z},
                 {"w", p.w}};
    }

    void from_json(const json &j, vec4 &p) {
        j.at("x").get_to(p.x);
        j.at("y").get_to(p.y);
        j.at("z").get_to(p.z);
        j.at("w").get_to(p.w);
    }

    void to_json(json &j, const quat &p) {
        j = json{{"x", p.x},
                 {"y", p.y},
                 {"z", p.z},
                 {"w", p.w}};
    }

    void from_json(const json &j, quat &p) {
        j.at("x").get_to(p.x);
        j.at("y").get_to(p.y);
        j.at("z").get_to(p.z);
        j.at("w").get_to(p.w);
    }

    // Matrices are flat arrays of their columns, older files have an object per column ("j0", "j1", ...)
    static void MatrixToJson(json& j, const float* values, int32_t count) {

        j = json::array();
        for (int32_t i = 0; i < count; i++)
            j.push_back(values[i]);

    }

    static bool MatrixFromJson(const json& j, float* values, int32_t count) {

        if (!j.is_array())
            return false;
        for (int32_t i = 0; i < count; i++)
            values[i] = j[i].get<float>();
        return true;

    }

    void to_json(json &j, const mat3 &p) {
        MatrixToJson(j, &p[0][0], 9);
    }

    void from_json(const json &j, mat3 &p) {
        if (MatrixFromJson(j, &p[0][0], 9))
            return;
        j.at("j0").get_to(p[0]);
        j.at("j1").get_to(p[1]);
        j.at("j2").get_to(p[2]);
    }

    void to_json(json &j, const mat4 &p) {
        MatrixToJson(j, &p[0][0], 16);
    }

    void from_json(const json &j, mat4 &p) {
        if (MatrixFromJson(j, &p[0][0], 16))
            return;
        j.at("j0").get_to(p[0]);
        j.at("j1").get_to(p[1]);
        j.at("j2").get_to(p[2]);
        j.at("j3").get_to(p[3]);
    }
}

namespace Atlas::Volume {

    void to_json(json& j, const Volume::AABB& p) {
        j = json{{"min", p.min}, {"max", p.max}};
    }

    void from_json(const json& j, Volume::AABB& p) {
        j.at("min").get_to(p.min);
        j.at("max").get_to(p.max);
    }

}