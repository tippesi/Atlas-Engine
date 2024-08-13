#pragma once

#include "System.h"
#include "scene/Entity.h"
#include "scene/components/LightComponent.h"
#include "scene/components/MeshComponent.h"

#include "graphics/Buffer.h"

#include <unordered_map>
#include <vector>
#include <atomic>
#include <mutex>
#include <future>

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

        struct EntityBatch {
            size_t count = 0;
            std::vector<ECS::Entity> entities;

            inline void Add(ECS::Entity entity) {
                if (count < entities.size())
                    entities[count] = entity;
                else
                    entities.push_back(entity);
                count++;
            }
        };

        struct Pass {
            RenderPassType type;

            Scene::Entity lightEntity;
            uint32_t layer;

            Ref<Scene::Scene> scene = nullptr;

            std::unordered_map<size_t, EntityBatch> meshToEntityMap;
            std::unordered_map<size_t, MeshInstances> meshToInstancesMap;
            std::unordered_map<size_t, ResourceHandle<Mesh::Mesh>> meshIdToMeshMap;

            bool wasUsed = false;

            std::vector<mat3x4> currentEntityMatrices;
            std::vector<mat3x4> lastEntityMatrices;
            std::vector<mat3x4> impostorMatrices;

            Ref<Graphics::MultiBuffer> currentMatricesBuffer;
            Ref<Graphics::MultiBuffer> lastMatricesBuffer;
            Ref<Graphics::MultiBuffer> impostorMatricesBuffer;

            void NewFrame(const Ref<Scene::Scene>& scene, const std::vector<ResourceHandle<Mesh::Mesh>>& meshes);

            void Add(const ECS::Entity& entity, const MeshComponent& meshComponent);

            void Update(vec3 cameraLocation);

            void FillBuffers();

            void Reset();
        };

        RenderList();

        ~RenderList();

        void NewFrame(const Ref<Scene::Scene>& scene);

        // Note: The expected behaviour is to first create and process all shadow passes and then finally do the main pass last
        Ref<Pass> NewMainPass();

        Ref<Pass> NewShadowPass(const ECS::Entity lightEntity, uint32_t layer);

        Ref<Pass> GetMainPass();

        Ref<Pass> GetShadowPass(const ECS::Entity lightEntity, const uint32_t layer);

        void FinishPass(const Ref<Pass>& pass);

        Ref<Pass> PopPassFromQueue(RenderPassType type);

        void Clear();

        Ref<Scene::Scene> scene = nullptr;

        std::vector<Ref<Pass>> passes;
        std::deque<Ref<Pass>> processedPasses;

        std::mutex mutex;
        std::atomic_bool doneProcessingShadows;

        JobGroup clearJob;
    };

}