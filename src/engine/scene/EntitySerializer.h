#pragma once

#include "components/ComponentSerializer.h"
#include "scene/Scene.h"

#include <set>

namespace Atlas::Scene {

    void EntityToJson(json& j, const Entity& p, Ref<Scene>& scene,
        std::set<ECS::Entity>& insertedEntites) {

        AE_ASSERT(!insertedEntites.contains(p) && "Entity should only be present once in the hierarchy");

        if (!insertedEntites.contains(p))
            insertedEntites.insert(p);

        j["id"] = size_t(p);

        if (p.HasComponent<NameComponent>()) {
            j["name"] = p.GetComponent<NameComponent>();
        }
        if (p.HasComponent<TransformComponent>()) {
            j["transform"] = p.GetComponent<TransformComponent>();
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
            // j["rigidBody"] = p.GetComponent<RigidBodyComponent>();
        }
        if (p.HasComponent<HierarchyComponent>()) {
            // Need to check how to get this to work
            auto& hierarchyComponent = p.GetComponent<HierarchyComponent>();
            std::vector<json> entities;
            for (auto entity : hierarchyComponent.GetChildren()) {
                entities.emplace_back();
                EntityToJson(entities.back(), entity, scene, insertedEntites);
            }
            j["entities"] = entities;
            j["root"] = hierarchyComponent.root;
        }
    }

    void EntityFromJson(const json& j, Entity& p, Ref<Scene>& scene) {
        size_t id = j["id"];

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

}