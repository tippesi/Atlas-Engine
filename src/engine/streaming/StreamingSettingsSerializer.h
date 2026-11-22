#pragma once

#include "common/SerializationHelper.h"

#include "StreamingSettings.h"

namespace Atlas::Streaming {

    void to_json(json& j, const StreamingSettings& p);

    void from_json(const json& j, StreamingSettings& p);

}
