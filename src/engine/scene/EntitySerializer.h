#pragma once

#include "components/ComponentSerializer.h"
#include "scene/Scene.h"

#include <set>

namespace Atlas::Scene {

    void EntityToJson(json& j, const Entity& p, Ref<Scene>& scene,
        std::set<ECS::Entity>& insertedEntites);

    void EntityFromJson(const json& j, Entity& p, Ref<Scene>& scene);
}