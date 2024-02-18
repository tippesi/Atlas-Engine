#pragma once

#include "common/SerializationHelper.h"

#include "AudioManager.h"
#include "resource/ResourceManager.h"

namespace Atlas::Audio {

    void to_json(json& j, const AudioStream& p);

    void from_json(const json& j, AudioStream& p);

}