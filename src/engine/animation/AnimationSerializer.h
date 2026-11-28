#pragma once

#include "Animation.h"

#include "common/SerializationHelper.h"

namespace Atlas::Animation {

    void to_json(json& j, const Channel& p);

    void from_json(const json& j, Channel& p);

    void to_json(json& j, const Animation& p);

    void from_json(const json& j, Animation& p);

}
