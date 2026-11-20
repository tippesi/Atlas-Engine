#pragma once

#include "components/ComponentSerializer.h"
#include "scene/Scene.h"

#include <set>

namespace Atlas::Scene {

    struct ComponentType {
        const char* key;
        const char* name;
    };

    // Every serialized component type with its key and a display name, in the order of the scene sections
    const std::vector<ComponentType>& GetComponentTypes();

    // Bit i is set when the entity has the component of GetComponentTypes()[i]
    uint32_t GetComponentMask(const Entity& p);

    // Copies of the component storages in the order of GetComponentTypes(), they only tell which entities have a component
    std::vector<ECS::Storage> CopyComponentStorages(Scene* scene);

    void RemoveComponent(Entity& p, const std::string& key);

    // Components of the entity without its id and children
    void ComponentsToJson(json& j, const Entity& p, Scene* scene);

    void EntityToJson(json& j, const Entity& p, Scene* scene,
        std::set<ECS::Entity>& insertedEntities);

    void EntityFromJson(const json& j, Entity& p, Scene* scene, 
        bool containsEntityMananger);

    // Scenes store their entities as a table of ids and parents and each component type in its own section
    void SceneToJson(json& j, Scene* scene);

    // Reads the sections and the nested entities of older scenes
    void SceneFromJson(const json& j, Ref<Scene>& scene);

    // Scene wide feature settings (sky, post processing, wind and the optional lighting features)
    void SceneSettingsToJson(json& j, const Scene* scene);

    void SceneSettingsFromJson(const json& j, Scene* scene);

    // Applies a JSON merge patch to the settings, missing features are created with default settings
    void PatchSceneSettings(const json& patch, Scene* scene);

    void EntityManagerToJson(json& j, const ECS::EntityManager& p);

    void EntityManagerFromJson(const json& j, ECS::EntityManager& p);

    void to_json(json& j, const Wind& p);

    void from_json(const json& j, Wind& p);

}
