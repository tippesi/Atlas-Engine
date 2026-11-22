#pragma once

#include "../System.h"
#include "Entity.h"
#include "common/Hash.h"

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

        // Each view has a counter per winding and level of detail of a mesh and one for its impostors, has to match
        // culling/common.hsh
        static constexpr uint32_t meshCounterCount = 2 * uint32_t(Mesh::MeshData::maxLodCount) + 1;
        // The culling result of an instance has a bit per view, has to match culling/common.hsh
        static constexpr uint32_t maxCullViewCount = 24;

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
            // The region of the indices of the mesh in each view, shared by its levels of detail, windings and impostors
            uint32_t regionOffset;
            uint32_t regionCapacity;
            uint32_t padding0 = 0;
            uint32_t lodCount;
            // Errors of the levels from 1 on divided by their pixel error
            vec4 lodErrors;
        };

        struct alignas(16) GPUCullView {
            vec4 planes[6];
            uint32_t shadow;
            uint32_t indexOffset;
            uint32_t padding0 = 0;
            uint32_t counterOffset;
        };

        struct alignas(16) GPUDrawRequest {
            uint32_t meshIdx;
            uint32_t view;
            uint32_t type;
            uint32_t indexCount;
            // In the buffers of the geometry pool, the draws bind them without offsets
            uint32_t firstIndex;
            uint32_t lod;
            int32_t vertexOffset;
            uint32_t padding0;
        };

        // The instances of a mesh, slot 0 is invalid
        struct MeshSlot {
            ResourceHandle<Mesh::Mesh> mesh;
            // Entities with a simulated soft body have their own slot of the mesh, the other instances keep their levels
            // of detail and impostors
            bool softBody = false;

            // Kept up to date by the refreshed instances
            std::vector<ECS::Entity> entities;
            uint32_t reversedWindingCount = 0;
            uint32_t dontCullCount = 0;
            // Grows with the instances where they are added, it only shrinks once all instances are gone
            Volume::AABB bounds;
            bool hasBounds = false;
            // Instances which are moving, so they don't grow the bounds along their way
            std::vector<ECS::Entity> movingEntities;
            // The bounds and the current bounds of the moving instances, set in every update
            Volume::AABB cullBounds;

            // Meshes without visible instances are left out of the first draws of an occluded pass, read back from the
            // occlusion test
            bool drawEarly = true;

            // Derived from the mesh in every update, its geometry and impostor can be streamed
            // Skinned meshes and simulated soft bodies, drawn by the host passes
            bool instanceVertices = false;
            // Loaded meshes without instance vertices, also the ones which aren't drawable: the streaming needs their
            // distances from the culling
            bool culled = false;
            // Meshes with translucent materials, their instances are culled and sorted by the host
            bool translucent = false;
            // The geometry of a streamed mesh can be unloaded, its instances are drawn as impostors then
            bool geometry = false;
            bool impostor = false;
            bool drawable = false;
            // The levels of detail the culling chooses from, each draw has a command for every one of them
            uint32_t lodCount = 1;
            uint32_t regionOffset = 0;
        };

        explicit GPUScene(Scene* scene);

        void Update(const std::vector<ECS::Entity>& dirtyEntities, const std::vector<ECS::Entity>& lastDirtyEntities);

        // Needs to be recorded before any other GPU work of the frame which reads the instances
        void UpdateBuffers(Graphics::CommandList* commandList);

        // Prepares the draws of a pass which is culled on the GPU, the skinned and soft body meshes are culled here
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

        // A culling of the GPU culled passes, its occlusion test continues it
        struct CullFrame {
            Ref<Graphics::Buffer> viewBuffer;
            Ref<Graphics::Buffer> requestBuffer;
            vec3 cameraLocation = vec3(0.0f);
            float focalLength = 0.0f;
            // The draws after the occlusion test have a view of their own
            uint32_t lateView = 0;
            VkDeviceSize occlusionViewOffset = 0;
            uint32_t lateFirstRequest = 0;
            uint32_t lateRequestCount = 0;
            bool readbackVisibility = false;

            // Whether the main pass has draws which wait for the occlusion test
            bool HasOcclusionDraws() const { return lateRequestCount > 0; }
        };

        // Writes the draw commands of the GPU culled passes of the render list
        CullFrame Cull(RenderList& renderList, vec3 cameraLocation, float focalLength, Graphics::CommandList* commandList);

        // Only computes the mesh distances, for rendering without any culled pass
        void CullForStreaming(vec3 cameraLocation, float focalLength, Graphics::CommandList* commandList);

        // Viewport height in pixels divided by 2 * tan(fov / 2) of the last culling, 0 before the first one
        float GetFocalLength() const;

        // Whether any mesh has translucent sub data, independent of the view so the passes for them don't toggle
        bool HasTranslucentMeshes() const { return hasTranslucentMeshes; }

        // Tests the instances of the main pass against the depth of its first draws and writes the commands of the
        // draws which are left, needs the hierarchical depth of these draws
        void CullOcclusion(const CullFrame& frame, const Ref<Texture::Texture2D>& hiZTexture, ivec2 depthResolution,
            int32_t hiZLevelCount, Graphics::CommandList* commandList);

        // Takes the newest mesh distances that arrived from the GPU, keeps the given ones if there are none
        void TakeMeshDistances(MeshDistances& meshDistances);

        // Writes the hardware ray tracing instances prepared by the last update and builds the acceleration structure
        void WriteRayTracingInstances(RayTracing::RayTracingWorld& rayTracingWorld, vec3 cameraLocation,
            float focalLength, Graphics::CommandList* commandList);

        void Bind(Graphics::CommandList* commandList) const;

        void Invalidate();

        std::vector<MeshSlot> slots = std::vector<MeshSlot>(1);

        // The instances of the draws of all views, impostors included
        Ref<Graphics::Buffer> indexBuffer;

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

        // Has to match culling/offsets.csh and culling/indices.csh
        struct IndexPushConstants {
            uint32_t firstView;
            uint32_t viewCount;
            uint32_t meshCount;
            uint32_t instanceCount;
        };

        static constexpr uint32_t notMoving = 0xFFFFFFFF;

        struct EntityState {
            uint32_t slot = 0;
            uint32_t slotPosition = 0;
            // Position in the moving entities of the slot
            uint32_t movingPosition = notMoving;
            bool reversedWinding = false;
            bool dontCull = false;
        };

        // The slots of the meshes take over which of their instances were visible in the last occlusion test
        void TakeVisibility();

        // The entities whose instances are written again, all of them after an invalidation
        void CollectRefreshEntities(const std::vector<ECS::Entity>& dirtyEntities,
            const std::vector<ECS::Entity>& lastDirtyEntities);

        void RefreshInstance(ECS::Entity entity);

        uint32_t GetSlot(const ResourceHandle<Mesh::Mesh>& mesh, bool softBody);

        void AddToSlot(ECS::Entity entity, EntityState& state, uint32_t slotIdx, bool reversedWinding, bool dontCull,
            const mat4& globalMatrix, bool moved);

        // An entity which stays in its slot keeps its position, otherwise it leaves and empty slots are freed
        void RemoveFromSlot(uint32_t entityIdx, EntityState& state, bool keepPosition);

        void RemoveFromMovingEntities(MeshSlot& slot, EntityState& state);

        void GrowBounds(MeshSlot& slot, const mat4& globalMatrix);

        static Volume::AABB GetInstanceBounds(const MeshSlot& slot, const mat4& globalMatrix);

        void UpdateMeshes();

        GPUCullMesh CreateCullMesh(MeshSlot& slot);

        // Sets the levels of detail of the slot and returns their errors for the culling
        vec4 SelectLods(MeshSlot& slot);

        // Instances which aren't culled on the GPU are culled here and drawn by the host pass
        void AddHostCulledInstances(const MeshSlot& slot, const Volume::Frustum& frustum, vec3 cameraLocation,
            RenderList::Pass& pass);

        void AddGPUDraws(uint32_t slotIdx, const Volume::Frustum& frustum, vec3 cameraLocation, RenderList::Pass& pass);

        // The readback of a frame copy was written FRAME_DATA_COUNT frames ago
        void ReadVisibility(int32_t frameIndex, uint32_t slotCount);

        void ReadMeshDistances(int32_t frameIndex, uint32_t slotCount);

        // A new buffer drops the copies that are still in flight
        void EnsureReadbackBuffer(Ref<Graphics::MultiBuffer>& buffer, size_t size,
            std::vector<size_t> (&frameMeshIds)[FRAME_DATA_COUNT]);

        void EnsureBuffer(Ref<Graphics::Buffer>& buffer, size_t size, VkBufferUsageFlags usage);

        void EnsureCullBuffers(size_t viewCount, size_t requestCount, bool occlusion = false);

        void BindCullBuffers(const Ref<Graphics::Buffer>& viewBuffer, Graphics::CommandList* commandList) const;

        // Turns the counts of the culling into offsets and writes the indices of the views, the occlusion test only
        // writes the instances it tested
        void WriteIndices(const CullFrame& frame, uint32_t firstView, uint32_t viewCount, bool occlusion,
            Graphics::CommandList* commandList);

        void WriteDrawCommands(const CullFrame& frame, uint32_t firstRequest, uint32_t requestCount,
            Graphics::CommandList* commandList);

        Scene* scene;

        Graphics::UploadBufferPool uploadBufferPool;

        // Simulated soft bodies of a mesh have a slot of their own
        struct SlotKey {
            Hash meshId;
            bool softBody;

            bool operator==(const SlotKey&) const = default;
        };
        struct SlotKeyHasher {
            size_t operator()(const SlotKey& key) const {
                Hash hash = key.meshId;
                HashCombine(hash, key.softBody);
                return hash;
            }
        };
        std::unordered_map<SlotKey, uint32_t, SlotKeyHasher> meshSlots;
        std::vector<uint32_t> freeSlots;

        struct DrawSortKey {
            Hash pipeline;
            const void* vertexBuffer;
            const void* indexBuffer;
            uint32_t draw;

            auto operator<=>(const DrawSortKey&) const = default;
        };
        std::vector<DrawSortKey> drawSortKeys;
        std::vector<RenderList::GPUDraw> sortedDraws;
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
        bool hasTranslucentMeshes = false;

        bool valid = false;
        bool clearInstances = false;

        // Whether a mesh slot had visible instances in the main pass, read back from the counters of the occlusion test
        Ref<Graphics::MultiBuffer> visibilityReadbackBuffer;
        std::vector<size_t> visibilityMeshIds[FRAME_DATA_COUNT];
        std::vector<size_t> visibleMeshIds;
        std::vector<uint8_t> visibleMeshes;

        Ref<Graphics::Buffer> instanceBuffer;
        Ref<Graphics::Buffer> historyBuffer;
        Ref<Graphics::Buffer> meshBuffer;
        Ref<Graphics::Buffer> counterBuffer;
        // The offsets of the draws in the index buffer, they end up behind the indices of their draws
        Ref<Graphics::Buffer> offsetBuffer;
        // The views each instance is visible in, see culling/cull.csh
        Ref<Graphics::Buffer> resultBuffer;
        Ref<Graphics::Buffer> commandBuffer;
        Ref<Graphics::Buffer> meshDistanceBuffer;
        Ref<Graphics::Buffer> occlusionInstanceBuffer;
        Ref<Graphics::MultiBuffer> meshDistanceReadbackBuffer;

        PipelineConfig scatterPipelineConfig;
        PipelineConfig cullPipelineConfig;
        PipelineConfig cullRayTracingPipelineConfig;
        PipelineConfig cullOcclusionPipelineConfig;
        PipelineConfig offsetsPipelineConfig;
        PipelineConfig indicesPipelineConfig;
        PipelineConfig indicesOcclusionPipelineConfig;
        PipelineConfig drawsPipelineConfig;

    };

}
