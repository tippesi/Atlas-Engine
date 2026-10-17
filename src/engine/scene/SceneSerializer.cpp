#include "SceneSerializer.h"

#include "lighting/LightingSerializer.h"
#include "audio/AudioSerializer.h"
#include "postprocessing/PostProcessingSerializer.h"
#include "physics/PhysicsSerializer.h"

#include "loader/TerrainLoader.h"

namespace Atlas::Scene {

    void EntityToJson(json& j, const Entity& p, Scene* scene,
        std::set<ECS::Entity>& insertedEntities) {

        if (!p.IsValid())
            return;

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
        if (p.HasComponent<SplineComponent>()) {
            j["spline"] = p.GetComponent<SplineComponent>();
        }
        if (p.HasComponent<HierarchyComponent>()) {
            // Need to check how to get this to work
            auto& hierarchyComponent = p.GetComponent<HierarchyComponent>();
            j["entities"] = json::array();
            auto& entities = j["entities"];

            for (auto entity : hierarchyComponent.GetChildren()) {
                entities.emplace_back();
                EntityToJson(entities.back(), entity, scene, insertedEntities);
            }
            j["root"] = hierarchyComponent.root;
        }
    }

    void EntityFromJson(const json& j, Entity& p, Scene* scene, bool containsEntityManager) {
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
        if (j.contains("spline")) {
            SplineComponent comp = j["spline"];
            p.AddComponent<SplineComponent>(comp);
        }
        if (j.contains("entities")) {
            // We need to first push back to a temporary vector to not invalidate
            // component references (i.e. when directly getting the hierarchy component).
            // That way all children will have components created before the parent creates its own
            std::vector<Entity> entities;
            entities.reserve(j["entities"].size());
            for (const auto& jEntity : j["entities"]) {
                Entity entity;
                if (containsEntityManager)
                    entity = Entity(jEntity["id"], &scene->entityManager);
                else
                    entity = scene->CreateEntity();
                EntityFromJson(jEntity, entity, scene, containsEntityManager);
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

        std::set<ECS::Entity> insertedEntities;

        j["entities"] = json::array();
        auto& entities = j["entities"];

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
            if (!entity.IsValid())
                continue;

            entities.emplace_back();
            EntityToJson(entities.back(), entity, scene, insertedEntities);
        }

        EntityManagerToJson(j["entityManager"], scene->entityManager);

        // Parse all mandatory members
        j["name"] = scene->name;
        j["aabb"] = scene->aabb;
        j["depth"] = scene->depth;

        SceneSettingsToJson(j, scene);

        // Parse all optional members
        if (scene->terrain.IsValid() && !scene->terrain.IsGenerated())
            j["terrain"] = scene->terrain.GetResource()->path;

        if (scene->physicsWorld)
            Physics::SerializePhysicsWorld(j["physicsWorld"], scene->physicsWorld);
       

    }

    void SceneFromJson(const json& j, Ref<Scene>& scene) {

        Volume::AABB aabb = j["aabb"];
        scene = CreateRef<Scene>(j["name"], aabb.min, aabb.max, j["depth"]);

         // Create physics world in any case (and before entities, such that they can push bodies immediately)
        scene->physicsWorld = CreateRef<Physics::PhysicsWorld>();
        scene->physicsWorld->pauseSimulation = true;

        bool containsEntityManager = j.contains("entityManager");
        if (containsEntityManager) {
            EntityManagerFromJson(j["entityManager"], scene->entityManager);
        }

        if (j.contains("physicsWorld")) {
            std::unordered_map<uint32_t, Physics::BodyCreationSettings> bodyCreationMap;
            Physics::DeserializePhysicsWorld(j["physicsWorld"], bodyCreationMap);
        }
        if (j.contains("terrain")) {
            scene->terrain = ResourceManager<Terrain::Terrain>::GetOrLoadResourceWithLoaderAsync(
                j["terrain"], Loader::TerrainLoader::LoadTerrain, false);
        }

        for (const auto& jEntity : j["entities"]) {
            Entity entity;
            if (containsEntityManager)
                entity = Entity(jEntity["id"], &scene->entityManager);
            else                
                entity = scene->CreateEntity();

            EntityFromJson(jEntity, entity, scene.get(), containsEntityManager);
        }

        SceneSettingsFromJson(j, scene.get());

        scene->rayTracingWorld = CreateRef<RayTracing::RayTracingWorld>();

        scene->physicsWorld->OptimizeBroadphase();

    }

    void SceneSettingsToJson(json& j, const Scene* scene) {

        j["sky"] = scene->sky;
        j["postProcessing"] = scene->postProcessing;
        j["wind"] = scene->wind;

        if (scene->fog)
            j["fog"] = *scene->fog;
        if (scene->irradianceVolume)
            j["irradianceVolume"] = *scene->irradianceVolume;
        if (scene->reflection)
            j["reflection"] = *scene->reflection;
        if (scene->rtgi)
            j["rtgi"] = *scene->rtgi;
        if (scene->sss)
            j["sss"] = *scene->sss;
        if (scene->ssgi)
            j["ssgi"] = *scene->ssgi;

    }

    static void ReadSceneSettings(const json& j, Scene* scene, bool mergePatch) {

        auto readSettings = [&]<class T>(const char* key, T& settings) {
            if (!j.contains(key))
                return;

            if (!mergePatch) {
                settings = j[key];
                return;
            }

            json merged = settings;
            merged.merge_patch(j[key]);
            settings = merged;
        };

        auto readFeature = [&]<class T>(const char* key, Ref<T>& feature) {
            if (!j.contains(key))
                return;

            if (!feature)
                feature = CreateRef<T>();
            readSettings(key, *feature);
        };

        readSettings("sky", scene->sky);
        readSettings("postProcessing", scene->postProcessing);
        readSettings("wind", scene->wind);

        readFeature("fog", scene->fog);
        readFeature("irradianceVolume", scene->irradianceVolume);
        readFeature("reflection", scene->reflection);
        readFeature("rtgi", scene->rtgi);
        readFeature("sss", scene->sss);
        readFeature("ssgi", scene->ssgi);

    }

    void SceneSettingsFromJson(const json& j, Scene* scene) {

        ReadSceneSettings(j, scene, false);

    }

    void PatchSceneSettings(const json& patch, Scene* scene) {

        ReadSceneSettings(patch, scene, true);

    }

    void EntityManagerToJson(json& j, const ECS::EntityManager& p) {

        j = json{
            {"entities", p.entities},
            {"destroyed", p.destroyed}
        };

    }

    void EntityManagerFromJson(const json& j, ECS::EntityManager& p) {

        try_get_json(j, "entities", p.entities);
        try_get_json(j, "destroyed", p.destroyed);

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
