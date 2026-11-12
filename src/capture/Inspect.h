#pragma once

#include "Common.h"

namespace Atlas::Capture {

    // inspect <file> [--filter <pattern>] [--assembly <pattern>] [--limit <n>] [--out <file.json>]
    // Reads files without loading them. Scenes and prefabs: entity and component counts, the most used meshes, entities
    // matching the filters with their position, and with --assembly the parts of an entity relative to it. Meshes
    // (.aemesh): bounds, where the origin lies, counts, materials, impostor and settings. Models (glTF, FBX, OBJ, ...):
    // nodes, meshes, bounds, materials, animations and the unit scale. Material graphs (.aematgraph): compile result,
    // parameters, nodes with their links and stages, --code the generated shaders. "nodes": the node library
    int32_t Inspect(const std::vector<std::string>& args);

}
