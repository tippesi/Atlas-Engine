#pragma once

#include "Job.h"
#include "SceneTools.h"

#include <set>

namespace Atlas::Capture {

    // Applies edit operations to a loaded scene. Operations selecting existing entities use name patterns or #ids
    // and by default require exactly one match to avoid accidental mass edits ("all": true allows more)
    // Throws std::runtime_error on invalid operations, the scene must not be saved in that case
    struct EditState {

        // Patched materials are only changed in memory, they are saved once the checks passed
        std::set<std::string> changedMaterials;
        // Terrain of the scene as it was loaded, it is only edited after a terrainCopy (or with "overwriteTerrain")
        std::string loadedTerrain;
        bool terrainChanged = false;
        // Operations which only make sense for inspection (detachTerrain) require a dry run
        bool dryRun = false;

    };

    json ApplyEditOperations(const Ref<Scene::Scene>& scene, const json& operations, EditState& state);

    // Model files (glTF, FBX, ...) are imported next to the source into meshes/ and materials/ first,
    // scenes can only reference .aemesh files
    ResourceHandle<Mesh::Mesh> LoadMeshForScene(const std::string& path);

    // An animation file or the name of an animation of the model file
    ResourceHandle<Animation::Animation> LoadAnimationForScene(const std::string& animation, const std::string& modelPath);

    // The root of the hierarchy, invalid for scenes without one (their entities are all top level)
    Scene::Entity FindRoot(Scene::Scene& scene);

    // Writes the materials to their files, a .backup of the original file is kept
    json SaveMaterials(const std::set<std::string>& paths);

    // Moves an entity to a world position, keeping its rotation (the parent transform is taken into account)
    void SetWorldPosition(Scene::Entity entity, vec3 position);

    // An invalid parent leaves the entity at the top level
    void AddChild(Scene::Entity parent, Scene::Entity entity);

    // Static body with the mesh as its collision shape
    void AddMeshBody(Scene::Entity entity, const ResourceHandle<Mesh::Mesh>& mesh, vec3 scale);

    // Point or spot light at the entity origin, the light of the add and scatter operations
    void AddLight(Scene::Entity entity, const json& spec);

    // Compares selected entity fields of two InspectScene results, including their rounded transform values
    json DiffInspections(const json& before, const json& after);

}
