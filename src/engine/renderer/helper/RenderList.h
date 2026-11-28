#pragma once

#include "System.h"
#include "scene/Entity.h"
#include "scene/components/LightComponent.h"
#include "scene/components/MeshComponent.h"

#include "graphics/Buffer.h"
#include "graphics/CommandList.h"
#include "graphics/UploadBufferPool.h"
#include "volume/Frustum.h"

#include <array>
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

            // Skinned meshes have the joint matrices of each instance one after another, soft body meshes the
            // offsets of the vertices of each instance
            size_t deformOffset = 0;

            // The instances of both windings are ordered by their level of detail
            std::array<uint32_t, Mesh::MeshData::maxLodCount> lodCounts = {};
            std::array<uint32_t, Mesh::MeshData::maxLodCount> reversedWindingLodCounts = {};
        };

        // An instance of a mesh with translucent sub data in the main pass, drawn one by one from far to near
        struct TranslucentInstance {
            size_t meshId;
            MeshInstances instances;
            bool softBody;
            float distance;
            Volume::AABB aabb;
        };

        enum class RenderPassType {
            Main = 0,
            Shadow = 1
        };

        // Translucent sub data is drawn by the translucent pass instead of the main one. Shadows are dithered by its
        // opacity, additive surfaces only add light and cast none
        static bool IsDrawnInPass(RenderPassType type, const Mesh::MeshSubData& subData) {
            const auto& material = subData.material;
            if (!material.IsLoaded())
                return false;
            return type == RenderPassType::Main ? !material->IsTranslucent() : material->GetBlendMode() != MaterialBlendMode::Additive;
        }

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
            // The first of the commands, there is one per level of detail
            uint32_t command = 0;
            uint32_t lodCount = 1;
        };

        // The draws of a pass culled on the GPU, their commands are written by the culling
        struct GPUDraws {
            bool culled = false;
            Volume::Frustum frustum;
            std::vector<GPUDraw> draws;
            // The same draws for the instances which are drawn after the occlusion test of the main pass
            std::vector<GPUDraw> lateDraws;

            void Reset();
        };

        struct Pass {
            RenderPassType type;

            Scene::Entity lightEntity;
            uint32_t layer;

            Scene::Scene* scene = nullptr;

            std::unordered_map<size_t, EntityBatch> meshToEntityMap;
            std::unordered_map<size_t, MeshInstances> meshToInstancesMap;
            // The instances of the batches which have a simulated soft body, drawn from their vertices
            std::unordered_map<size_t, MeshInstances> meshToSoftBodyInstancesMap;
            // Sorted from the farthest to the closest instance, only in the main pass
            std::vector<TranslucentInstance> translucentInstances;

            bool wasUsed = false;

            // Shadow passes keep their shadow map while the hash of their instances stays the one it was drawn with.
            // The renderer only keeps final shadow maps (see ShadowRenderer), others and passes with casters which
            // move their vertices (skinning, wind, soft bodies) aren't hashed (0)
            Hash contentHash = 0;
            Hash drawnContentHash = 0;
            bool keepsShadowMap = true;
            bool hasDeformingCasters = false;

            std::vector<mat3x4> currentEntityMatrices;
            std::vector<mat3x4> lastEntityMatrices;
            std::vector<mat3x4> currentJointMatrices;
            std::vector<mat3x4> lastJointMatrices;
            std::vector<uint32_t> softBodyVertexOffsets;

            // With the instance buffer entities which aren't skinned are referenced by their index
            std::vector<uint32_t> instanceIndices;
            std::vector<uint32_t> impostorInstanceIndices;

            // Passes culled on the GPU only have the skinned meshes and soft bodies in the host instances above
            GPUDraws gpu;

            Ref<Graphics::MultiBuffer> currentMatricesBuffer;
            Ref<Graphics::MultiBuffer> lastMatricesBuffer;
            Ref<Graphics::MultiBuffer> currentJointMatricesBuffer;
            Ref<Graphics::MultiBuffer> lastJointMatricesBuffer;
            Ref<Graphics::Buffer> softBodyVertexOffsetsBuffer;
            Ref<Graphics::UploadBufferPool> uploadPool = CreateRef<Graphics::UploadBufferPool>();
            Ref<Graphics::MultiBuffer> instanceIndicesBuffer;
            Ref<Graphics::MultiBuffer> impostorInstanceIndicesBuffer;

            RenderPassThreadContext contexts[8];

            // The levels of detail of the entities of a batch while it is updated
            std::vector<std::pair<uint32_t, ECS::Entity>> entityLods;

            void NewFrame(Scene::Scene* scene, const std::vector<ResourceHandle<Mesh::Mesh>>& meshes,
                const std::unordered_map<size_t, ResourceHandle<Mesh::Mesh>>& meshIdToMeshMap);

            void Add(const ECS::Entity& entity, const MeshComponent& meshComponent);

            void Add(int32_t threadIdx, const ECS::Entity& entity, const MeshComponent& meshComponent);

            void Finalize();

            void Update(vec3 cameraLocation, RenderList& renderList);

            void FillBuffers();

            void Reset();
        };

        RenderList();

        ~RenderList();

        void NewFrame(Scene::Scene* scene, const std::vector<ResourceHandle<Mesh::Mesh>>& meshes);

        // Needs the simulated vertices of the frame, before the passes are filled
        void UpdateSoftBodyVertices(Scene::Scene* scene);

        // Note: The expected behaviour is to first create and process all shadow passes and then finally do the main pass last
        Ref<Pass> NewMainPass();

        Ref<Pass> NewShadowPass(const ECS::Entity lightEntity, uint32_t layer);

        Ref<Pass> GetMainPass();

        Ref<Pass> GetShadowPass(const ECS::Entity lightEntity, const uint32_t layer);

        void FinishPass(const Ref<Pass>& pass, RenderPassType type);

        void AddGPUCulledPass(const Ref<Pass>& pass);

        // Draws with the command written by the GPU culling, empty draws are skipped on the GPU
        void DrawGPUCulled(Graphics::CommandList* commandList, const GPUDraw& draw) const;

        // Draws the instances of one winding of a host culled mesh, a draw per level of detail, beforeDraw(lod) is
        // called before each draw. Meshes drawn from the buffers of the geometry pool add their first index and vertex
        template<class BeforeDraw>
        static void DrawInstances(Graphics::CommandList* commandList, const Mesh::MeshSubData& subData,
            const MeshInstances& instances, bool reversedWinding, uint32_t firstIndex, int32_t vertexOffset,
            const BeforeDraw& beforeDraw) {

            const auto& lodCounts = reversedWinding ? instances.reversedWindingLodCounts : instances.lodCounts;
            auto firstInstance = uint32_t(reversedWinding ? instances.reversedWindingOffset : instances.offset);
            for (int32_t lod = 0; lod <= int32_t(subData.lods.size()); lod++) {
                if (!lodCounts[lod])
                    continue;

                beforeDraw(uint32_t(lod));
                auto range = subData.GetLod(lod);
                commandList->DrawIndexed(range.indicesCount, lodCounts[lod], firstIndex + range.indicesOffset, vertexOffset,
                    firstInstance);
                firstInstance += lodCounts[lod];
            }

        }

        // The level of detail of an instance in the host culled passes, with the settings of the frame like the GPU
        // culling chooses it
        uint32_t GetLod(const Mesh::Mesh& mesh, const mat4& globalMatrix, const Volume::AABB& bounds, vec3 cameraLocation);

        Ref<Pass> PopPassFromQueue(RenderPassType type);

        void Clear();

        Scene::Scene* scene = nullptr;

        std::vector<Ref<Pass>> passes;

        std::deque<Ref<Pass>> processedMainPasses;
        std::deque<Ref<Pass>> processedShadowPasses;

        // Passes of the current frame which are culled on the GPU and the buffer with their draw commands
        std::vector<Ref<Pass>> gpuCulledPasses;
        Ref<Graphics::Buffer> gpuCommandBuffer;

        // Vertices of the simulated soft bodies (position with the packed normal in w) of the frame for all passes
        std::vector<vec4> currentSoftBodyVertices;
        std::vector<vec4> lastSoftBodyVertices;
        std::unordered_map<ECS::Entity, uint32_t> entityToSoftBodyVertices;
        Ref<Graphics::Buffer> currentSoftBodyVerticesBuffer;
        Ref<Graphics::Buffer> lastSoftBodyVerticesBuffer;
        // Each upload gets its own buffer, the passes bind exactly the buffers of their frame
        Graphics::UploadBufferPool uploadPool;

        std::mutex mutex;
        std::atomic_bool wasCleared = false;

        std::unordered_map<size_t, ResourceHandle<Mesh::Mesh>> meshIdToMeshMap;

        JobGroup clearJob { "Clear render list", JobPriority::High };

    private:
        float lodFocalLength = 0.0f;
        float lodErrorScale = 1.0f;
        int32_t forcedLod = -1;
    };

}
