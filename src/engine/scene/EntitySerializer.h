#pragma once

#include "components/ComponentSerializer.h"
#include "scene/Scene.h"

namespace Atlas::Scene {

    void EntityToJson(json& j, const Entity& p, Ref<Scene>& scene) {

        if (p.HasComponent<NameComponent>()) {
            j["name"] = p.GetComponent<NameComponent>();
        }
        if (p.HasComponent<TransformComponent>()) {
            j["transform"] = p.GetComponent<NameComponent>();
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
            // j["mesh"] = p.GetComponent<RigidBodyComponent>();
        }
        if (p.HasComponent<HierarchyComponent>()) {
            // Need to check how to get this to work

        }
    }

    void EntityFromJson(const json& j, Entity& p, Ref<Scene>& scene) {
        if (j.contains("name")) {
            NameComponent comp = j["name"];
        }
        if (j.contains("transform")) {

        }
        if (j.contains("mesh")) {
            MeshComponent comp = j["mesh"];
            p.AddComponent<MeshComponent>(std::move(comp));
        }
        if (j.contains("light")) {

        }
        if (j.contains("camera")) {

        }
        if (j.contains("audio")) {

        }
        if (j.contains("audioVolume")) {

        }
        if (j.contains("rigidBody")) {

        }
        if (j.contains("hierarchy")) {

        }
    }

}