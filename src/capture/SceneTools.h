#pragma once

#include "Common.h"

#include "scene/Scene.h"

#include <functional>
#include <unordered_set>

namespace Atlas::Capture {

    using EntitySet = std::unordered_set<ECS::Entity>;

    // Physics shapes can differ from visible meshes and terrain
    constexpr Scene::SceneQueryComponents VisualComponents = Scene::MeshComponentBit | Scene::TerrainComponentBit;

    // Ground, clearance and paths: what characters collide with. Bodies are hit with their collision shapes,
    // meshes without a mesh shape body (e.g. grass) directly, so scenes without physics still work
    constexpr Scene::SceneQueryComponents CollisionComponents = VisualComponents | Scene::RigidBodyComponentBit;

    struct RaycastHit {

        bool valid = false;
        float distance = 0.0f;
        vec3 position = vec3(0.0f);
        vec3 normal = vec3(0.0f);
        Scene::Entity entity;
        // terrain, mesh, body, text
        std::string kind;
        // Index of the hit triangle for mesh hits
        int64_t triangle = -1;

    };

    // Material path of the hit triangle, or the submesh name if it has no material; empty if unknown
    std::string GetHitMaterial(const RaycastHit& hit);

    // Label of the hit entity, or the kind of the hit if no entity was hit (e.g. terrain)
    std::string GetHitLabel(const RaycastHit& hit);

    // Converts a pixel of a camera described by a job camera spec into a world space ray (origin, direction)
    using PickRayFunction = std::function<std::pair<vec3, vec3>(const json& camera, vec2 pixel)>;

    std::string GetEntityName(Scene::Entity entity);

    // Unique label used in reports, e.g. "Gate (#123)"
    std::string GetEntityLabel(Scene::Entity entity);

    std::vector<Scene::Entity> FindEntities(Scene::Scene& scene, const std::vector<std::string>& patterns);

    std::vector<Scene::Entity> FindEntities(Scene::Scene& scene, const std::string& pattern);

    // Adds the entity and all of its descendants to the set
    void CollectHierarchy(Scene::Entity entity, EntitySet& set);

    // Union of the loaded mesh bounds of the entity and its descendants. Entities without
    // loaded meshes get a small box around their position
    Volume::AABB GetEntityBounds(Scene::Entity entity, bool* hasMesh = nullptr);

    Volume::AABB GetBounds(const std::vector<Scene::Entity>& entities);

    Volume::AABB GetSceneBounds(Scene::Scene& scene);

    vec3 GetEntityPosition(Scene::Entity entity);

    // Yaw of the entity's world rotation in radians (rotation around +Y, 0 means facing +Z)
    float GetEntityYaw(Scene::Entity entity);

    // Scene::CastRay with triangle precision, filtering ignored entities, players and invisible meshes
    RaycastHit Raycast(Scene::Scene& scene, vec3 origin, vec3 direction, float maxDistance,
        const EntitySet& ignored = {}, Scene::SceneQueryComponents components = VisualComponents);

    // Casts downwards from the given height
    RaycastHit GroundBelow(Scene::Scene& scene, vec3 position, const EntitySet& ignored = {});

    json InspectScene(Scene::Scene& scene, const std::vector<std::string>& filter,
        const std::vector<std::string>& detail, const EntitySet& excluded = {});

    json ExportSettings(Scene::Scene& scene);

    void ApplySettings(Scene::Scene& scene, const json& settings);

    // Merge patch with the fields of the .aematerial files, null removes a map
    void PatchMaterial(Material& material, const json& patch);

    // Patches all loaded materials whose path matches a pattern, e.g. { "kingdom/*_01_A*": { "metalness": 0.2 } }
    // The fields are the ones of the .aematerial files. Returns the number of patched materials
    int32_t ApplyMaterialPatches(const json& patches);

    // Runs a list of spatial queries, maps and routes are written into the output directory
    json RunQueries(Scene::Scene& scene, const json& queries, const PickRayFunction& pickRay = {}, const fs::path& outputDirectory = {});

}
