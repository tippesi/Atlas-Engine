#pragma once

#include "System.h"
#include "scene/Entity.h"
#include "scene/components/LightComponent.h"
#include "scene/components/MeshComponent.h"

#include "graphics/Buffer.h"
#include "volume/Frustum.h"

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

            // Instances with reversed winding occupy the suffix of the mesh's matrix range.
            size_t reversedWindingOffset = 0;
            size_t reversedWindingCount = 0;

            size_t impostorOffset;
            size_t impostorCount;

            // Skinned meshes have the joint matrices of each instance one after another
            size_t jointOffset = 0;
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

        struct RenderPassThreadContext {
            std::vector<std::pair<ECS::Entity, Hash>> entities;
        };

        enum class GPUDrawType : uint32_t {
            DefaultWinding = 0,
            ReversedWinding = 1,
            Impostor = 2
        };

        // Indexed and non-indexed draw commands have the same stride
        static constexpr uint32_t gpuDrawCommandSize = 5 * sizeof(uint32_t);

        // A draw of the instances which are culled on the GPU, its command is written there
        struct GPUDraw {
            // Impostor draws don't have sub data
            Mesh::MeshSubData* subData = nullptr;
            Mesh::Mesh* mesh = nullptr;
            Hash meshId = 0;
            uint32_t slot = 0;
            GPUDrawType type = GPUDrawType::DefaultWinding;
            uint32_t command = 0;
        };

        struct Pass {
            RenderPassType type;

            Scene::Entity lightEntity;
            uint32_t layer;

            Scene::Scene* scene = nullptr;

            std::unordered_map<size_t, EntityBatch> meshToEntityMap;
            std::unordered_map<size_t, MeshInstances> meshToInstancesMap;

            bool wasUsed = false;

            std::vector<mat3x4> currentEntityMatrices;
            std::vector<mat3x4> lastEntityMatrices;
            std::vector<mat3x4> impostorMatrices;
            std::vector<mat3x4> currentJointMatrices;
            std::vector<mat3x4> lastJointMatrices;

            // With the instance buffer entities which aren't skinned are referenced by their index
            std::vector<uint32_t> instanceIndices;
            std::vector<uint32_t> impostorInstanceIndices;

            // Passes culled on the GPU only have the skinned meshes in the lists above
            bool gpuCulled = false;
            Volume::Frustum frustum;
            std::vector<GPUDraw> gpuDraws;

            Ref<Graphics::MultiBuffer> currentMatricesBuffer;
            Ref<Graphics::MultiBuffer> lastMatricesBuffer;
            Ref<Graphics::MultiBuffer> impostorMatricesBuffer;
            Ref<Graphics::MultiBuffer> currentJointMatricesBuffer;
            Ref<Graphics::MultiBuffer> lastJointMatricesBuffer;
            Ref<Graphics::MultiBuffer> instanceIndicesBuffer;
            Ref<Graphics::MultiBuffer> impostorInstanceIndicesBuffer;

            RenderPassThreadContext contexts[8];

            void NewFrame(Scene::Scene* scene, const std::vector<ResourceHandle<Mesh::Mesh>>& meshes,
                const std::unordered_map<size_t, ResourceHandle<Mesh::Mesh>>& meshIdToMeshMap);

            void Add(const ECS::Entity& entity, const MeshComponent& meshComponent);

            void Add(int32_t threadIdx, const ECS::Entity& entity, const MeshComponent& meshComponent);

            void Finalize();

            void Update(vec3 cameraLocation, const std::unordered_map<size_t, ResourceHandle<Mesh::Mesh>>& meshIdToMeshMap);

            void FillBuffers();

            void Reset();
        };

        RenderList();

        ~RenderList();

        void NewFrame(Scene::Scene* scene, const std::vector<ResourceHandle<Mesh::Mesh>>& meshes);

        // Note: The expected behaviour is to first create and process all shadow passes and then finally do the main pass last
        Ref<Pass> NewMainPass();

        Ref<Pass> NewShadowPass(const ECS::Entity lightEntity, uint32_t layer);

        Ref<Pass> GetMainPass();

        Ref<Pass> GetShadowPass(const ECS::Entity lightEntity, const uint32_t layer);

        void FinishPass(const Ref<Pass>& pass, RenderPassType type);

        void AddGPUCulledPass(const Ref<Pass>& pass);

        // Draws with the command written by the GPU culling, empty draws are skipped on the GPU
        void DrawGPUCulled(Graphics::CommandList* commandList, const GPUDraw& draw) const;

        Ref<Pass> PopPassFromQueue(RenderPassType type);

        void Clear();

        Scene::Scene* scene = nullptr;

        std::vector<Ref<Pass>> passes;

        std::deque<Ref<Pass>> processedMainPasses;
        std::deque<Ref<Pass>> processedShadowPasses;

        // Passes of the current frame which are culled on the GPU and the buffer with their draw commands
        std::vector<Ref<Pass>> gpuCulledPasses;
        Ref<Graphics::Buffer> gpuCommandBuffer;

        std::mutex mutex;
        std::atomic_bool wasCleared = false;

        std::unordered_map<size_t, ResourceHandle<Mesh::Mesh>> meshIdToMeshMap;

        JobGroup clearJob { "Clear render list", JobPriority::High };
    };

}
