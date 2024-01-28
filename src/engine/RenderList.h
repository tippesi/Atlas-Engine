#pragma once

#include "System.h"
#include "scene/Entity.h"
#include "scene/components/LightComponent.h"

#include "graphics/Buffer.h"

#include <map>
#include <vector>

namespace Atlas {

    namespace Scene {
        class Scene;
    }

    class RenderList {

    public:
        struct MeshInstances {
            size_t offset;
            size_t count;

            size_t impostorOffset;
            size_t impostorCount;
        };

        enum class RenderPassType {
            Main = 0,
            Shadow = 1
        };

        struct Pass {
            RenderPassType type;

            ECS::Entity lightEntity;
            uint32_t layer;

            std::map<size_t, std::vector<ECS::Entity>> meshToEntityMap;
            std::map<size_t, MeshInstances> meshToInstancesMap;
            std::map<size_t, ResourceHandle<Mesh::Mesh>> meshIdToMeshMap;
        };

        RenderList();

        void NewFrame(Ref<Scene::Scene> scene);

        void NewMainPass();

        void NewShadowPass(const ECS::Entity lightEntity, uint32_t layer);

        Pass* GetMainPass();

        Pass* GetShadowPass(const ECS::Entity lightEntity, const uint32_t layer);

        void Add(const ECS::Entity& entity, const MeshComponent& meshComponent);

        void Update(vec3 cameraLocation);

        void FillBuffers();

        Ref<Scene::Scene> scene = nullptr;

        std::vector<mat3x4> currentEntityMatrices;
        std::vector<mat3x4> lastEntityMatrices;
        std::vector<mat3x4> impostorMatrices;

        Ref<Graphics::MultiBuffer> currentMatricesBuffer;
        Ref<Graphics::MultiBuffer> lastMatricesBuffer;
        Ref<Graphics::MultiBuffer> impostorMatricesBuffer;

        std::vector<Pass> passes;

    };

}