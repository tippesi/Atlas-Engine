#include "SceneSerializer.h"

#include "lighting/LightingSerializer.h"
#include "audio/AudioSerializer.h"
#include "postprocessing/PostProcessingSerializer.h"
#include "physics/PhysicsSerializer.h"

namespace Atlas::Scene {

    void EntityToJson(json& j, const Entity& p, Scene* scene,
        std::set<ECS::Entity>& insertedEntities) {

        AE_ASSERT(!insertedEntities.contains(p) && "Entity should only be present once in the hierarchy");

        if (!insertedEntities.contains(p))
            insertedEntities.insert(p);

        j["id"] = size_t(p);

        if (p.HasComponent<NameComponent>()) {
            j["name"] = p.GetComponent<NameComponent>();
        }
        if (p.HasComponent<TransformComponent>()) {
            // Create a copy and update local matrix (physics always just update global matrices)
            auto transformComponent = p.GetComponent<TransformComponent>();
            auto parentEntity = scene->GetParentEntity(p);
            transformComponent.ReconstructLocalMatrix(parentEntity);
            j["transform"] = transformComponent;
        }
        if (p.HasComponent<MeshComponent>()) {
            j["mesh"] = p.GetComponent<MeshComponent>();
        }
        if (p.HasComponent<LightComponent>()) {
            j["light"] = p.GetComponent<LightComponent>();
        }
        if (p.HasComponent<CameraComponent>()) {
            j["camera"] = p.GetComponent<CameraComponent>();
        }
        if (p.HasComponent<AudioComponent>()) {
            j["audio"] = p.GetComponent<AudioComponent>();
        }
        if (p.HasComponent<AudioVolumeComponent>()) {
            j["audioVolume"] = p.GetComponent<AudioVolumeComponent>();
        }
        if (p.HasComponent<RigidBodyComponent>()) {
            j["rigidBody"] = p.GetComponent<RigidBodyComponent>();
        }
        if (p.HasComponent<PlayerComponent>()) {
            j["player"] = p.GetComponent<PlayerComponent>();
        }
        if (p.HasComponent<TextComponent>()) {
            j["text"] = p.GetComponent<TextComponent>();
        }
        if (p.HasComponent<LuaScriptComponent>()) {
            j["script"] = p.GetComponent<LuaScriptComponent>();
        }
        if (p.HasComponent<HierarchyComponent>()) {
            // Need to check how to get this to work
            auto& hierarchyComponent = p.GetComponent<HierarchyComponent>();
            std::vector<json> entities;
            for (auto entity : hierarchyComponent.GetChildren()) {
                entities.emplace_back();
                EntityToJson(entities.back(), entity, scene, insertedEntities);
            }
            j["entities"] = entities;
            j["root"] = hierarchyComponent.root;
        }
    }

    void EntityFromJson(const json& j, Entity& p, Scene* scene) {
        if (j.contains("name")) {
            NameComponent comp = j["name"];
            p.AddComponent<NameComponent>(comp);
        }
        if (j.contains("transform")) {
            TransformComponent comp = j["transform"];
            p.AddComponent<TransformComponent>(comp);
        }
        if (j.contains("mesh")) {
            MeshComponent comp = j["mesh"];
            p.AddComponent<MeshComponent>(std::move(comp));
        }
        if (j.contains("light")) {
            LightComponent comp = j["light"];
            p.AddComponent<LightComponent>(comp);
        }
        if (j.contains("camera")) {
            CameraComponent comp = j["camera"];
            p.AddComponent<CameraComponent>(comp);
        }
        if (j.contains("audio")) {
            AudioComponent comp = j["audio"];
            p.AddComponent<AudioComponent>(comp);
        }
        if (j.contains("audioVolume")) {
            AudioVolumeComponent comp = j["audioVolume"];
            p.AddComponent<AudioVolumeComponent>(comp);
        }
        if (j.contains("rigidBody")) {
            RigidBodyComponent comp = j["rigidBody"];
            p.AddComponent<RigidBodyComponent>(comp);
        }
        if (j.contains("player")) {
            PlayerComponent comp = j["player"];
            p.AddComponent<PlayerComponent>(comp);
        }
        if (j.contains("text")) {
            TextComponent comp = j["text"];
            p.AddComponent<TextComponent>(comp);
        }
        if (j.contains("script")) {
            LuaScriptComponent comp = j["script"];
            p.AddComponent<LuaScriptComponent>(comp);
        }
        if (j.contains("entities")) {
            // We need to first push back to a temporary vector to not invalidate
            // component references (i.e. when directly getting the hierarchy component).
            // That way all children will have components created before the parent creates its own
            std::vector<json> jEntities = j["entities"];
            std::vector<Entity> entities;
            for (auto jEntity : jEntities) {
                auto entity = scene->CreateEntity();
                EntityFromJson(jEntity, entity, scene);
                entities.push_back(entity);
            }

            auto& comp = p.AddComponent<HierarchyComponent>();
            comp.root = j["root"];
            for (auto entity : entities) {
                comp.AddChild(entity);
            }
        }
    }

    void SceneToJson(json& j, Scene* scene) {

        std::vector<json> entities;
        std::set<ECS::Entity> insertedEntities;

        auto hierarchySubset = scene->GetSubset<HierarchyComponent>();
        for (auto entity : hierarchySubset) {
            auto hierarchy = hierarchySubset.Get(entity);
            // Serialize from root recursively. Also means free floating hierarchies without a
            // root will be ignored.
            if (!hierarchy.root) continue;
            entities.emplace_back();
            EntityToJson(entities.back(), entity, scene, insertedEntities);
        }

        for (auto entity : *scene) {
            // No need here, since it was already inserted through the hierarchy
            if (insertedEntities.contains(entity))
                continue;

            entities.emplace_back();
            EntityToJson(entities.back(), entity, scene, insertedEntities);
        }

        // Parse all mandatory members
        j["name"] = scene->name;
        j["aabb"] = scene->aabb;
        j["depth"] = scene->depth;
        j["entities"] = entities;
        j["sky"] = scene->sky;
        j["postProcessing"] = scene->postProcessing;
        j["wind"] = scene->wind;

        // Parse all optional members
        if (scene->fog)
            j["fog"] = *scene->fog;
        if (scene->irradianceVolume)
            j["irradianceVolume"] = *scene->irradianceVolume;
        if (scene->ao)
            j["ao"] = *scene->ao;
        if (scene->reflection)
            j["reflection"] = *scene->reflection;
        if (scene->rtgi)
            j["rtgi"] = *scene->rtgi;
        if (scene->sss)
            j["sss"] = *scene->sss;
        if (scene->ssgi)
            j["ssgi"] = *scene->ssgi;

        if (scene->physicsWorld)
            Physics::SerializePhysicsWorld(j["physicsWorld"], scene->physicsWorld);

    }

    void SceneFromJson(const json& j, Ref<Scene>& scene) {

        Volume::AABB aabb = j["aabb"];
        scene = CreateRef<Scene>(j["name"], aabb.min, aabb.max, j["depth"]);

         // Create physics world in any case (and before entities, such that they can push bodies immediately)
        scene->physicsWorld = CreateRef<Physics::PhysicsWorld>();
        scene->physicsWorld->pauseSimulation = true;

        if (j.contains("physicsWorld")) {
            std::unordered_map<uint32_t, Physics::BodyCreationSettings> bodyCreationMap;
            Physics::DeserializePhysicsWorld(j["physicsWorld"], bodyCreationMap);
        }

        std::vector<json> jEntities = j["entities"];
        for (auto jEntity : jEntities) {
            auto entity = scene->CreateEntity();
            EntityFromJson(jEntity, entity, scene.get());
        }

        scene->sky = j["sky"];
        scene->postProcessing = j["postProcessing"];

        if (j.contains("fog")) {
            scene->fog = CreateRef<Lighting::Fog>();
            *scene->fog = j["fog"];
        }
        if (j.contains("irradianceVolume")) {
            scene->irradianceVolume = CreateRef<Lighting::IrradianceVolume>();
            *scene->irradianceVolume = j["irradianceVolume"];
        }
        if (j.contains("ao")) {
            scene->ao = CreateRef<Lighting::AO>();
            *scene->ao = j["ao"];
        }
        if (j.contains("reflection")) {
            scene->reflection = CreateRef<Lighting::Reflection>();
            *scene->reflection = j["reflection"];
        }
        if (j.contains("rtgi")) {
            scene->rtgi = CreateRef<Lighting::RTGI>();
            *scene->rtgi = j["rtgi"];
        }
        if (j.contains("sss")) {
            scene->sss = CreateRef<Lighting::SSS>();
            *scene->sss = j["sss"];
        }
        if (j.contains("ssgi")) {
            scene->ssgi = CreateRef<Lighting::SSGI>();
            *scene->ssgi = j["ssgi"];
        }
        if (j.contains("wind")) {
            scene->wind = j["wind"];
        }

        scene->rayTracingWorld = CreateRef<RayTracing::RayTracingWorld>();

        scene->physicsWorld->OptimizeBroadphase();

    }

    void to_json(json& j, const Wind& p) {

        j = json{
            {"direction", p.direction},
            {"speed", p.speed}
        };

    }

    void from_json(const json& j, Wind& p) {

        try_get_json(j, "direction", p.direction);
        try_get_json(j, "speed", p.speed);

    }

}