#include "AnimationSerializer.h"

namespace Atlas::Animation {

    void to_json(json& j, const Channel& p) {

        j = json{
            {"jointName", p.jointName},
        };

        set_binary_json(j, "positionTimes", p.positionTimes);
        set_binary_json(j, "positions", p.positions);
        set_binary_json(j, "rotationTimes", p.rotationTimes);
        set_binary_json(j, "rotations", p.rotations);
        set_binary_json(j, "scaleTimes", p.scaleTimes);
        set_binary_json(j, "scales", p.scales);

    }

    void from_json(const json& j, Channel& p) {

        j.at("jointName").get_to(p.jointName);

        try_get_binary_json(j, "positionTimes", p.positionTimes);
        try_get_binary_json(j, "positions", p.positions);
        try_get_binary_json(j, "rotationTimes", p.rotationTimes);
        try_get_binary_json(j, "rotations", p.rotations);
        try_get_binary_json(j, "scaleTimes", p.scaleTimes);
        try_get_binary_json(j, "scales", p.scales);

    }

    void to_json(json& j, const Animation& p) {

        j = json{
            {"name", p.name},
            {"duration", p.duration},
            {"channels", p.channels},
        };

    }

    void from_json(const json& j, Animation& p) {

        j.at("name").get_to(p.name);
        j.at("duration").get_to(p.duration);
        j.at("channels").get_to(p.channels);

    }

}
