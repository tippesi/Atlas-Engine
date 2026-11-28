#pragma once

#include "common/SerializationHelper.h"

#include "Project.h"

namespace Atlas {

    void to_json(json& j, const Project& p);

    void from_json(const json& j, Project& p);

}
