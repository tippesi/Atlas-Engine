#include "SerializationHelper.h"

namespace glm {

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
        j.at("w").get_to(p.z);
    }

    void to_json(json &j, const mat3 &p) {
        json j0, j1, j2;
        to_json(j0, p[0]);
        to_json(j1, p[1]);
        to_json(j2, p[2]);
        j = json{{"j0", j0},
                 {"j1", j1},
                 {"j2", j2}};
    }

    void from_json(const json &j, mat3 &p) {
        json j0, j1, j2;
        j.at("j0").get_to(j0);
        j.at("j1").get_to(j1);
        j.at("j2").get_to(j2);

        from_json(j0, p[0]);
        from_json(j1, p[1]);
        from_json(j2, p[2]);
    }

    void to_json(json &j, const mat4 &p) {
        json j0, j1, j2, j3;
        to_json(j0, p[0]);
        to_json(j1, p[1]);
        to_json(j2, p[2]);
        to_json(j3, p[3]);
        j = json{{"j0", j0},
                 {"j1", j1},
                 {"j2", j2},
                 {"j3", j3}};
    }

    void from_json(const json &j, mat4 &p) {
        json j0, j1, j2, j3;
        j.at("j0").get_to(j0);
        j.at("j1").get_to(j1);
        j.at("j2").get_to(j2);
        j.at("j3").get_to(j3);

        from_json(j0, p[0]);
        from_json(j1, p[1]);
        from_json(j2, p[2]);
        from_json(j3, p[3]);
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