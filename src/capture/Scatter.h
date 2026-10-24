#pragma once

#include "SceneTools.h"

#include <functional>

namespace Atlas::Capture {

    // Scatters mesh instances over the terrain into a new group entity, see the "scatter" edit operation
    // in README.md. Placement is deterministic for a given seed. Returns placement statistics
    json Scatter(Scene::Scene& scene, const json& spec);

    // Point test for a scatter area (circle or ring, rectangle, polygon or path corridor) in x/z
    std::function<bool(vec2)> MakeAreaTest(const json& area);

}
