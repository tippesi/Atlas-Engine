#pragma once

#include "Job.h"
#include "SceneTools.h"

namespace Atlas::Capture {

    // Applies edit operations to a loaded scene. Operations selecting existing entities use name patterns or #ids
    // and by default require exactly one match to avoid accidental mass edits ("all": true allows more)
    // Throws std::runtime_error on invalid operations, the scene must not be saved in that case
    json ApplyEditOperations(Scene::Scene& scene, const json& operations);

    // Moves an entity to a world position, keeping its rotation (the parent transform is taken into account)
    void SetWorldPosition(Scene::Entity entity, vec3 position);

    // Compares selected entity fields of two InspectScene results, including their rounded transform values
    json DiffInspections(const json& before, const json& after);

}
