#pragma once

#include "components/ComponentSerializer.h"
#include "scene/Scene.h"

#include <set>

namespace Atlas::Scene {

    void EntityToJson(json& j, const Entity& p, Scene* scene,
        std::set<ECS::Entity>& insertedEntities);

    void EntityFromJson(const json& j, Entity& p, Scene* scene, 
        bool containsEntityMananger);

    void SceneToJson(json& j, Scene* scene);

    void SceneFromJson(const json& j, Ref<Scene>& scene);

    void EntityManagerToJson(json& j, const ECS::EntityManager& p);

    void EntityManagerFromJson(const json& j, ECS::EntityManager& p);

    void to_json(json& j, const Wind& p);

    void from_json(const json& j, Wind& p);

}