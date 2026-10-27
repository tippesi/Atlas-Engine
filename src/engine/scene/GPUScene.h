#pragma once

#include "../System.h"
#include "Entity.h"

#include "mesh/Mesh.h"
#include "volume/AABB.h"
#include "volume/Frustum.h"
#include "graphics/Buffer.h"
#include "graphics/CommandList.h"
#include "graphics/UploadBufferPool.h"
#include "pipeline/PipelineConfig.h"
#include "renderer/helper/RenderList.h"

#include <unordered_map>
#include <vector>

namespace Atlas::RayTracing {

    class RayTracingWorld;

}

namespace Atlas::Scene {

    class Scene;

    /**
     * The instances of all mesh components on the GPU, indexed by entity and updated for the entities which changed.
     * The main and the directional shadow passes are culled from them on the GPU, the hardware ray tracing instances
     * are written from them by the same shader. Skinned meshes stay culled on the host.
     */
    class GPUScene {

    public:
        enum InstanceFlags : uint32_t {
            InstanceVisible = 1,
            InstanceDontCull = 2,
            InstanceReversedWinding = 4
        };

        enum MeshFlags : uint32_t {
            MeshDrawable = 1,
            MeshCastShadow = 2,
            MeshImpostor = 4
        };

        static constexpr uint32_t invalidHistoryIdx = 0xFFFFFFFF;

        // Each view has a counter per draw type of a mesh, has to match culling/common.hsh
        static constexpr uint32_t drawTypeCount = 3;

        // Has to match the structures in culling/common.hsh, GPUDrawRequest the one in culling/draws.csh
        struct alignas(16) GPUInstance {
            // Rows of the global matrix
            mat3x4 matrix = mat3x4(1.0f);
            uint32_t meshIdx = 0;
            uint32_t flags = 0;
            uint32_t historyIdx = invalidHistoryIdx;
            uint32_t padding0 = 0;
        };

        struct alignas(16) GPUCullMesh {
            // The w components are the culling distance and the shadow culling distance
            vec4 aabbMin;
            vec4 aabbMax;
            float impostorDistance;
            float impostorShadowDistance;
            uint32_t flags;
            uint32_t padding0;
            uint32_t regionOffset;
            uint32_t regionCapacity;
            uint32_t impostorRegionOffset;
            uint32_t padding1;
        };

        struct alignas(16) GPUCullView {
            vec4 planes[6];
            uint32_t shadow;
            uint32_t indexOffset;
            uint32_t impostorIndexOffset;
            uint32_t counterOffset;
        };

        struct alignas(16) GPUDrawRequest {
            uint32_t meshIdx;
            uint32_t view;
            uint32_t type;
            uint32_t indexCount;
            uint32_t firstIndex;
            uint32_t padding0;
            uint32_t padding1;
            uint32_t padding2;
        };

        // The instances of a mesh, slot 0 is invalid
        struct MeshSlot {
            ResourceHandle<Mesh::Mesh> mesh;
            std::vector<ECS::Entity> entities;

            uint32_t reversedWindingCount = 0;
            uint32_t dontCullCount = 0;

            // Grows with the instances, it only shrinks once all instances are gone
            Volume::AABB bounds;
            bool hasBounds = false;

            // State of the current frame
            bool drawable = false;
            bool skinned = false;
            bool impostor = false;
            uint32_t regionOffset = 0;
            uint32_t impostorRegionOffset = 0;
        };

        explicit GPUScene(Scene* scene);

        void Update(const std::vector<ECS::Entity>& dirtyEntities, const std::vector<ECS::Entity>& lastDirtyEntities);

        // Needs to be recorded before any other GPU work of the frame which reads the instances
        void UpdateBuffers(Graphics::CommandList* commandList);

        // Prepares the draws of a pass which is culled on the GPU, the skinned meshes are culled here
        void FillPass(const Volume::Frustum& frustum, vec3 cameraLocation, const Ref<RenderList::Pass>& pass);

        // Writes the draw commands of the GPU culled passes of the render list
        void Cull(RenderList& renderList, vec3 cameraLocation, Graphics::CommandList* commandList);

        // Writes the hardware ray tracing instances prepared by the last update and builds the acceleration structure
        void WriteRayTracingInstances(RayTracing::RayTracingWorld& rayTracingWorld, vec3 cameraLocation,
            Graphics::CommandList* commandList);

        void Bind(Graphics::CommandList* commandList) const;

        void Invalidate();

        std::vector<MeshSlot> slots = std::vector<MeshSlot>(1);

        Ref<Graphics::Buffer> indexBuffer;
        Ref<Graphics::Buffer> impostorIndexBuffer;

    private:
        // Has to match culling/cull.csh
        struct alignas(16) CullPushConstants {
            vec4 cameraLocation;
            uint32_t instanceCount;
            uint32_t viewCount;
            uint32_t maxRayTracingInstanceCount = 0;
            uint32_t includeObjectHistory = 0;
        };

        struct EntityState {
            uint32_t slot = 0;
            uint32_t slotPosition = 0;
            bool reversedWinding = false;
            bool dontCull = false;
        };

        uint32_t GetSlot(const ResourceHandle<Mesh::Mesh>& mesh);

        void RemoveFromSlot(uint32_t entityIdx, EntityState& state);

        void GrowBounds(MeshSlot& slot, const mat4& globalMatrix);

        void UpdateMeshes();

        void EnsureBuffer(Ref<Graphics::Buffer>& buffer, size_t size, VkBufferUsageFlags usage);

        void EnsureCullBuffers(size_t viewCount, size_t requestCount);

        void BindCullBuffers(const Ref<Graphics::Buffer>& viewBuffer, Graphics::CommandList* commandList) const;

        Scene* scene;

        Graphics::UploadBufferPool uploadBufferPool;

        std::unordered_map<Hash, uint32_t> meshSlots;
        std::vector<uint32_t> freeSlots;
        std::vector<EntityState> entityStates;

        std::vector<ECS::Entity> refreshEntities;
        std::vector<GPUInstance> updates;
        std::vector<uint32_t> updateIndices;
        std::vector<mat3x4> history;
        std::vector<GPUCullMesh> cullMeshes;

        // The instance buffer is indexed by entity
        uint32_t instanceCount = 0;
        uint32_t totalCapacity = 0;
        uint32_t impostorCapacity = 0;

        bool valid = false;
        bool clearInstances = false;

        Ref<Graphics::Buffer> instanceBuffer;
        Ref<Graphics::Buffer> historyBuffer;
        Ref<Graphics::Buffer> meshBuffer;
        Ref<Graphics::Buffer> counterBuffer;
        Ref<Graphics::Buffer> commandBuffer;

        PipelineConfig scatterPipelineConfig;
        PipelineConfig cullPipelineConfig;
        PipelineConfig cullRayTracingPipelineConfig;
        PipelineConfig drawsPipelineConfig;

    };

}
