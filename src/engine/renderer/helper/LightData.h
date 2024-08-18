#pragma once

#include "../../System.h"

#include <vector>

#include "scene/Scene.h"

namespace Atlas::Renderer::Helper {

    class LightData {

    public:
        struct LightEntity {
            Scene::Entity entity;
            LightComponent comp;
            int32_t mapIdx = -1;
        };

        LightData() = default;

        void CullAndSort(const Ref<Scene::Scene>& scene);

        void UpdateBindlessIndices(const Ref<Scene::Scene>& scene);

        void FillBuffer(const Ref<Scene::Scene>& scene);

        Buffer::Buffer lightBuffer;

        std::vector<LightEntity> lightEntities;

    };

}