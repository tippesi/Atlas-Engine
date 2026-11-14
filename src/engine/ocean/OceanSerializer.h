#pragma once

#include "common/SerializationHelper.h"

#include "Ocean.h"

namespace Atlas::Ocean {

    void to_json(json& j, const Ocean& p);

    void from_json(const json& j, Ocean& p);

}
