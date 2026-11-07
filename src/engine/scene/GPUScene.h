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

#include <atomic>
#include <unordered_map>
#include <vector>
#include <mutex>

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
            MeshImpostor = 4,
            MeshGeometry = 8,
            MeshDrawEarly = 16
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
            // New instances are drawn first, the occlusion test of the same frame corrects this
            uint32_t visibleLastFrame = 1;
        };

        // Updates of an instance with this value keep the result of its last occlusion test, has to match culling/scatter.csh
        static constexpr uint32_t keepVisibleLastFrame = 2;

        struct alignas(16) GPUCullMesh {
            // The w components are the culling distance and the shadow culling distance
            vec4 aabbMin;
            vec4 aabbMax;
            float impostorDistance;
            float impostorShadowDistance;
            uint32_t flags;
            float minScreenSize;
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

            // Meshes without visible instances are left out of the first draws of an occluded pass
            bool drawEarly = true;

            // State of the current frame
            bool drawable = false;
            bool skinned = false;
            bool impostor = false;
            // The geometry of a streamed mesh can be unloaded, its instances are drawn as impostors then
            bool geometry = false;
            uint32_t regionOffset = 0;
            uint32_t impostorRegionOffset = 0;
        };

        explicit GPUScene(Scene* scene);

        void Update(const std::vector<ECS::Entity>& dirtyEntities, const std::vector<ECS::Entity>& lastDirtyEntities);

        // Needs to be recorded before any other GPU work of the frame which reads the instances
        void UpdateBuffers(Graphics::CommandList* commandList);

        // Prepares the draws of a pass which is culled on the GPU, the skinned meshes are culled here
        void FillPass(const Volume::Frustum& frustum, vec3 cameraLocation, const Ref<RenderList::Pass>& pass);

        // Closest distances of the instances of each mesh slot, computed by the culling. NaN without such an instance
        struct MeshDistances {
            // The mesh of each slot when the distances were computed
            std::vector<size_t> meshIds;
            // Of the instances drawn with geometry, to their bounds in units of their scale
            std::vector<float> distances;
            // Of all visible instances, to their positions or centers, whichever is closer
            std::vector<float> positionDistances;
        };

        // Writes the draw commands of the GPU culled passes of the render list
        void Cull(RenderList& renderList, vec3 cameraLocation, float focalLength, Graphics::CommandList* commandList);

        // Only computes the mesh distances, for rendering without any culled pass
        void CullForStreaming(vec3 cameraLocation, float focalLength, Graphics::CommandList* commandList);

        // Viewport height in pixels divided by 2 * tan(fov / 2) of the last culling, 0 before the first one
        float GetFocalLength() const;

        // Whether the main pass of the last culling has draws which wait for the occlusion test
        bool HasOcclusionDraws() const { return lateRequestCount > 0; }

        // Tests the instances of the main pass against the depth of its first draws and writes the commands of the
        // draws which are left, needs the hierarchical depth of these draws
        void CullOcclusion(const Ref<Texture::Texture2D>& hiZTexture, ivec2 depthResolution, int32_t hiZLevelCount,
            Graphics::CommandList* commandList);

        // Takes the newest mesh distances that arrived from the GPU, keeps the given ones if there are none
        void TakeMeshDistances(MeshDistances& meshDistances);

        // Writes the hardware ray tracing instances prepared by the last update and builds the acceleration structure
        void WriteRayTracingInstances(RayTracing::RayTracingWorld& rayTracingWorld, vec3 cameraLocation,
            float focalLength, Graphics::CommandList* commandList);

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
            float focalLength = 0.0f;
            uint32_t occlusionView = 0xFFFFFFFF;
            uint32_t firstView = 0;
            uint32_t padding0 = 0;
            ivec2 depthResolution = ivec2(0);
            int32_t hiZLevelCount = 0;
            uint32_t padding1 = 0;
        };

        // Has to match culling/draws.csh
        struct DrawsPushConstants {
            uint32_t firstRequest;
            uint32_t requestCount;
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

        void EnsureCullBuffers(size_t viewCount, size_t requestCount, bool occlusion = false);

        void BindCullBuffers(const Ref<Graphics::Buffer>& viewBuffer, Graphics::CommandList* commandList) const;

        void WriteDrawCommands(uint32_t firstRequest, uint32_t requestCount, Graphics::CommandList* commandList);

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
        std::vector<size_t> cullMeshIds;

        // The distances that each frame copy of the readback buffer receives
        std::vector<size_t> distanceMeshIds[FRAME_DATA_COUNT];
        std::atomic<float> lastFocalLength = 0.0f;
        int32_t distanceFrameIndex = -1;

        // The culling of a render thread writes them while the scene update takes them
        std::mutex meshDistancesMutex;
        MeshDistances meshDistances;
        bool hasMeshDistances = false;

        // The instance buffer is indexed by entity
        uint32_t instanceCount = 0;
        uint32_t totalCapacity = 0;
        uint32_t impostorCapacity = 0;

        bool valid = false;
        bool clearInstances = false;

        // Whether a mesh slot had visible instances in the main pass, read back from the counters of the occlusion test
        Ref<Graphics::MultiBuffer> visibilityReadbackBuffer;
        std::vector<size_t> visibilityMeshIds[FRAME_DATA_COUNT];
        std::vector<size_t> visibleMeshIds;
        std::vector<uint8_t> visibleMeshes;
        bool readbackVisibility = false;

        // The last culling, the occlusion test continues it
        Ref<Graphics::Buffer> cullViewBuffer;
        Ref<Graphics::Buffer> cullRequestBuffer;
        vec3 cullCameraLocation = vec3(0.0f);
        float cullFocalLength = 0.0f;
        uint32_t lateView = 0;
        VkDeviceSize occlusionViewOffset = 0;
        uint32_t lateFirstRequest = 0;
        uint32_t lateRequestCount = 0;

        Ref<Graphics::Buffer> instanceBuffer;
        Ref<Graphics::Buffer> historyBuffer;
        Ref<Graphics::Buffer> meshBuffer;
        Ref<Graphics::Buffer> counterBuffer;
        Ref<Graphics::Buffer> commandBuffer;
        Ref<Graphics::Buffer> meshDistanceBuffer;
        Ref<Graphics::Buffer> occlusionInstanceBuffer;
        Ref<Graphics::MultiBuffer> meshDistanceReadbackBuffer;

        PipelineConfig scatterPipelineConfig;
        PipelineConfig cullPipelineConfig;
        PipelineConfig cullRayTracingPipelineConfig;
        PipelineConfig cullOcclusionPipelineConfig;
        PipelineConfig drawsPipelineConfig;

    };

}
