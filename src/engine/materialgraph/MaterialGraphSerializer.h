#pragma once

#include "MaterialGraph.h"
#include "../common/SerializationHelper.h"

namespace Atlas::MaterialGraph {

    void to_json(json& j, const CodeInput& p);

    void from_json(const json& j, CodeInput& p);

    void to_json(json& j, const Node& p);

    void from_json(const json& j, Node& p);

    void to_json(json& j, const Link& p);

    void from_json(const json& j, Link& p);

    void to_json(json& j, const Graph& p);

    void from_json(const json& j, Graph& p);

}
