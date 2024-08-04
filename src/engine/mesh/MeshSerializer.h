#pragma once

#include "Mesh.h"

#include "common/SerializationHelper.h"

#include <string>
#include <unordered_map>

namespace Atlas::Mesh {

    void to_json(json& j, const Impostor& p);

    void from_json(const json& j, Impostor& p);

    void to_json(json& j, const Mesh& p);

    void from_json(const json& j, Mesh& p);

}