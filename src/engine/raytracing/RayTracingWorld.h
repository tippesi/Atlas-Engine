#pragma once

#include "System.h"

#include "RTStructures.h"
#include "BLAS.h"
#include "scene/Subset.h"


#include "terrain/TerrainNode.h"
#include "scene/components/MeshComponent.h"
#include "scene/components/TransformComponent.h"
#include "buffer/VertexBuffer.h"
#include "graphics/UploadBufferPool.h"
#include "pipeline/PipelineConfig.h"
#include "materialgraph/MaterialGraph.h"

#include "texture/TextureAtlas.h"

#include <vector>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace Atlas {

    // Forward-declare class to use it as a friend
    namespace Renderer::Helper {
        class RayTracingHelper;
    }

    namespace Scene {

        class Scene;
        class SceneRenderState;

    }

    namespace RayTracing {

        class RayTracingWorld {

            friend Renderer::Helper::RayTracingHelper;
            friend Scene::Scene;
            friend Scene::SceneRenderState;

        public:
            // All deformation jobs of one pipeline are dispatched together
            struct DeformDispatch {
                PipelineConfig config;
                uint32_t groupOffset = 0;
                uint32_t groupCount = 0;
            };

            struct DeformBlas {
                Ref<Graphics::BLAS> blas;
                VkDeviceAddress vertexAddress = 0;
                bool refit = false;
            };

            // The deformed triangles of a software BLAS are refit level by level
            struct SoftwareDeformBlas {
                Ref<BLAS> blas;
                Buffer::Buffer levelNodeBuffer;
                Buffer::Buffer nodeBoundsBuffer;
                std::vector<uint32_t> levelOffsets;
                // A new BLAS starts as a copy of the mesh BLAS, the deformation keeps its topology
                Ref<BLAS> copySource;
            };

            // The deformation of the last update, recorded while rendering since it needs the global descriptor sets
            struct PendingDeformation {
                std::vector<DeformDispatch> dispatches;
                std::vector<DeformBlas> blases;
                std::vector<SoftwareDeformBlas> softwareBlases;
                Ref<Graphics::Buffer> jobBuffer;
                Ref<Graphics::Buffer> groupJobBuffer;
                Ref<Graphics::Buffer> jointMatrixBuffer;
                Ref<Graphics::Buffer> softBodyVertexBuffer;
                Ref<Graphics::Buffer> triangleJobBuffer;
                Ref<Graphics::Buffer> triangleGroupJobBuffer;
                uint32_t triangleGroupCount = 0;
                // Dropped if the frame of the update isn't rendered
                int32_t frameIndex = -1;
            };

            struct MaterialGraphHitGroup {
                std::string closestHitFile;
                std::string anyHitFile;

                auto operator<=>(const MaterialGraphHitGroup&) const = default;
            };

            // Instances prepared by the update, the GPU scene writes its own ones while culling
            struct PendingInstances {
                Ref<Graphics::Buffer> rtMeshBuffer;
                Ref<Graphics::Buffer> hostInstanceBuffer;
                Ref<Graphics::Buffer> hostBvhInstanceBuffer;
                Ref<Graphics::Buffer> hostLastMatricesBuffer;
                uint32_t hostInstanceCount = 0;
                uint32_t gpuInstanceCount = 0;
                uint32_t instanceBound = 0;
                bool includeObjectHistory = false;
                // The upload buffers are only valid in the frame of the update
                int32_t frameIndex = -1;
                // Replaces the TLAS of the world once it is built, an update that isn't rendered keeps the last one
                Ref<Graphics::TLAS> tlas;

                // The hit records and graph hit groups the instances were written with
                Ref<const std::vector<uint32_t>> hitRecords;
                Ref<const std::vector<MaterialGraphHitGroup>> materialGraphHitGroups;
            };

            RayTracingWorld();

            // The dirty entities were created, moved or changed in this update
            void Update(Scene::Subset<MeshComponent, TransformComponent> subset, const std::vector<ECS::Entity>& dirtyEntities);

            // The GPU scene writes its hardware ray tracing instances next to the ones prepared by the update and builds
            // the acceleration structure with the methods below. Returns nullptr if the current frame has no update.
            const PendingInstances* GetPendingInstances() const;

            // Deforms the instances of the last update into their BLASes, has to be recorded before any tracing
            void RecordDeformation(Graphics::CommandList* commandList);

            void PrepareInstances(Graphics::CommandList* commandList);

            void BindInstanceOutputs(Graphics::CommandList* commandList);

            void FinishInstances(Graphics::CommandList* commandList);

            bool IsValid();

            void Clear();

            // The shader binding table of a ray tracing pipeline with the hit records of the last prepared instances,
            // changed records are copied in the command list
            const Graphics::ShaderBindingTable& GetShaderBindingTable(const Ref<Graphics::Pipeline>& pipeline,
                Graphics::CommandList* commandList);

            // The config of a ray tracing pipeline with the hit groups of the material graphs of the last prepared
            // instances, nullptr without them. Hit group i + 1 runs the shaders of graph hit group i.
            PipelineConfig* GetMaterialGraphPipelineConfig(const PipelineConfig& config);

            bool includeObjectHistory = false;

            // Hardware ray tracing refits the BLASes of deformed meshes instead of building them every frame,
            // software ray tracing always refits them
            bool refitSkinnedBlases = true;

            // Animated meshes further away from the camera are traced in their rest pose, skinning and building
            // a BLAS for every animated entity of a large scene each frame is too expensive
            float skinnedBlasDistance = 60.0f;

            // Instances of static meshes moved by wind or a material graph are deformed within this distance, the GPU
            // scene leaves them out and they're written on the host
            float deformedBlasDistance = 30.0f;

        private:
            struct BlasInfo {
                ResourceHandle<Mesh::Mesh> mesh;
                Terrain::TerrainNode* node = nullptr;

                int32_t offset = 0;
                int32_t materialOffset = 0;
                // Block of hit records with the hit group of each geometry, only meshes with graphs have one
                uint32_t hitRecordOffset = 0;
                uint32_t hitRecordCount = 0;
                // Geometries with graphs aren't opaque in the BLAS, the instances are if none of the graphs has an opacity
                bool forceOpaque = false;

                float cullingDistanceSqr = 0.0f;

                int32_t idx = 0;
                // The last update that found the BLAS
                uint64_t update = 0;
            };

            // Nearby animated meshes are deformed into a BLAS of the entity, hardware ray tracing hits read everything
            // but the normals from the mesh, software ray tracing refits a copy of the mesh BLAS
            struct DeformedInstance {
                ResourceHandle<Mesh::Mesh> mesh;
                // Mesh::BuildBVH creates a new BLAS for changed data, which invalidates the deformed one
                Ref<BLAS> meshBlas;
                Ref<Graphics::BLAS> blas;
                Buffer::Buffer normalBuffer;
                // The BLAS reads the indices of the mesh, buffers created again (e.g. for levels of detail) invalidate it
                Ref<Graphics::Buffer> indexBuffer;

                Ref<BLAS> softwareBlas;
                Buffer::Buffer nodeBoundsBuffer;

                // A static mesh of the GPU scene, deformed by wind or a material graph
                bool deformedStatic = false;
                // Further away the data is kept for a while, but the mesh BLAS is traced
                bool active = false;
                bool used = false;
            };

            // The pipeline and graph parameters of a sub mesh, the same for all instances of a mesh (or all simulated
            // soft bodies of it)
            struct SubDataDeformation {
                PipelineConfig config;
                Hash key = 0;
                uint32_t materialGraphOffset = 0;
                // Set up again when the graph changes or while its pipeline compiles
                Ref<const MaterialGraph::CompiledGraph> graph;
                bool pending = false;
                bool usesGraph = false;
            };

            // Undeformed vertices of a mesh and the vertices of each sub mesh as first vertex and count
            struct DeformMesh {
                Ref<BLAS> meshBlas;
                // The vertices are copied from the data, they change with the buffers of the mesh
                Ref<Graphics::Buffer> meshVertexBuffer;
                std::vector<SubDataDeformation> deformations;
                std::vector<SubDataDeformation> softBodyDeformations;
                Buffer::Buffer vertexBuffer;
                std::vector<std::pair<uint32_t, uint32_t>> subDataVertexRanges;

                // Software ray tracing: the vertex indices of the triangles in the order of the mesh BLAS and its
                // nodes by tree level, level i are levelOffsets[i] to levelOffsets[i + 1] in the level node buffer
                Buffer::Buffer triangleVertexBuffer;
                Buffer::Buffer levelNodeBuffer;
                std::vector<uint32_t> levelOffsets;
            };

            struct alignas(16) JobPushConstants {
                uint32_t groupOffset;
                uint32_t groupCount;
            };

            struct alignas(16) RefitPushConstants {
                uint32_t nodeOffset;
                uint32_t nodeCount;
            };

            // Has to match RT_MESH_USABLE in culling/rtinstances.hsh
            static constexpr uint32_t rtMeshUsable = 1;
            static constexpr uint32_t rtMeshForceOpaque = 2;

            // Until the instance count is read back, at most this many GPU instances are added to the host ones
            static constexpr uint32_t initialGPUInstanceBound = 1u << 18;
            static constexpr uint32_t instanceBoundMargin = 1024;

            // Ray tracing data of the mesh slots of the GPU scene, has to match culling/rtinstances.hsh
            struct alignas(16) GPURTMesh {
                uint32_t blasOffset = 0;
                int32_t materialOffset = 0;
                uint32_t mask = 0;
                uint32_t flags = 0;
                float cullingDistanceSqr = 0.0f;
                float deformDistanceSqr = 0.0f;
                VkDeviceAddress blasAddress = 0;
                uint32_t hitRecordOffset = 0;
                // The levels of detail from 1 on follow each other from this mesh on, lodCount includes level 0
                uint32_t lodOffset = 0;
                uint32_t lodCount = 1;
                uint32_t padding0 = 0;
            };

            struct JobContext {
                std::vector<GPUBVHInstance> gpuBvhInstances;
                std::vector<Volume::AABB> instanceAABBs;
                std::vector<mat3x4> lastMatrices;
            };

            void UpdateMaterials();

            void AllocateHitRecords(BlasInfo& blasInfo, uint32_t count);

            void ReleaseHitRecords(BlasInfo& blasInfo);

            void UpdateForSoftwareRayTracing(vec3 cameraLocation, bool hasCamera);

            void UpdateForHardwareRayTracing(vec3 cameraLocation, bool hasCamera);

            void BuildSoftwareTLAS(std::vector<GPUBVHInstance>& gpuBvhInstances,
                std::vector<mat3x4>& lastMatrices, std::vector<Volume::AABB>& instanceAABBs);

            void UpdateDeformedInstances(const std::vector<ECS::Entity>& dirtyEntities);

            // Static meshes deformed by wind or their material near the camera, see deformCandidates
            void UpdateDeformCandidates(const std::vector<ECS::Entity>& dirtyEntities, vec3 cameraLocation, bool hasCamera);

            DeformedInstance CreateDeformedInstance(const ResourceHandle<Mesh::Mesh>& mesh);

            void CreateSoftwareTopology(DeformMesh& deformMesh, const ResourceHandle<Mesh::Mesh>& mesh);

            bool IsDeformedByMaterial(const ResourceHandle<Mesh::Mesh>& mesh) const;

            Scene::Scene* scene;

            Graphics::ASBuilder tlasBuilder;
            Graphics::ASBuilder blasBuilder;
            Ref<Graphics::TLAS> tlas;

            std::vector<Ref<Graphics::BLAS>> buildBlases;

            Buffer::Buffer materialBuffer;
            Buffer::Buffer bvhInstanceBuffer;
            Buffer::Buffer tlasNodeBuffer;
            Buffer::Buffer lastMatricesBuffer;

            std::vector<VkAccelerationStructureInstanceKHR> hardwareInstances;
            std::vector<GPUBVHInstance> gpuBvhInstances;
            std::vector<Volume::AABB> instanceAABBs;
            std::vector<mat3x4> lastMatrices;

            std::vector<mat3x4> jointMatrices;
            std::vector<vec4> softBodyVertices;

            std::unordered_map<ECS::Entity, DeformedInstance> deformedInstances;
            std::unordered_map<size_t, DeformMesh> deformMeshes;
            // Meshes of the GPU scene with instances written on the host within the deformation distance
            std::unordered_set<size_t> deformedMeshes;
            // Entities of deformed meshes near the camera, see UpdateDeformedInstances
            std::vector<ECS::Entity> deformCandidates;
            vec3 deformCandidateLocation = vec3(0.0f);
            size_t deformCandidateSlotCount = 0;
            int32_t deformCandidateFrames = 0;
            // Positions and with software ray tracing also normals, only read while the BLASes are built or refit
            Ref<Graphics::Buffer> deformVertexBuffer;
            PendingDeformation pendingDeformation;
            // Builds while rendering, separate from the scratch memory of the update
            Graphics::ASBuilder deformBlasBuilder;

            PipelineConfig deformTrianglesPipelineConfig;
            PipelineConfig refitBVHPipelineConfig;

            // Hardware instances are written on the GPU, the ones of the host first
            std::vector<GPURTMesh> rtMeshes;

            struct PipelineShaderBindingTable {
                Ref<Graphics::Pipeline> pipeline;
                Graphics::ShaderBindingTable table;
                Ref<const std::vector<uint32_t>> hitRecords;
                uint64_t lastRequest = 0;
            };

            // Hit group per record of the hit region, traces advance one record per geometry. Meshes without graphs
            // share the first records, the blocks of the others stay where they are while they have graphs
            std::vector<uint32_t> hitRecords;
            std::vector<std::pair<uint32_t, uint32_t>> freeHitRecords;
            uint32_t defaultHitRecordCount = 0;
            uint64_t hitRecordsVersion = 0;
            // The hit records and opaque instances only change with the BLASes, the graphs and their hit groups
            Hash hitRecordsKey = 0;
            bool blasInfosChanged = true;
            // Shaders of the active material graphs which compiled
            Ref<const std::vector<MaterialGraphHitGroup>> materialGraphHitGroups;
            Ref<const std::vector<uint32_t>> hitRecordsSnapshot;
            uint64_t hitRecordsSnapshotVersion = 0;

            // Render side copies, taken over when the instances are prepared
            Ref<const std::vector<uint32_t>> renderHitRecords;
            Ref<const std::vector<MaterialGraphHitGroup>> renderMaterialGraphHitGroups;
            std::unordered_map<Hash, PipelineConfig> materialGraphPipelineConfigs;

            std::vector<PipelineShaderBindingTable> shaderBindingTables;
            uint64_t shaderBindingTableRequests = 0;
            std::optional<PendingInstances> pendingInstances;
            Ref<Graphics::Buffer> tlasInstanceBuffer;
            Ref<Graphics::Buffer> instanceCounterBuffer;
            Ref<Graphics::MultiBuffer> instanceCountReadback;
            // The readback holds a count once every frame slot was written
            uint32_t readbackFrameCount = 0;
            Graphics::UploadBufferPool uploadBufferPool;

            // Kept from update to update, the infos of BLASes that are gone are removed
            std::unordered_map<Ref<BLAS>, BlasInfo> blasInfos;
            uint64_t blasInfoUpdate = 0;

            std::vector<GPUMaterial> materials;
            std::vector<JobContext> jobContexts;

            bool hardwareRayTracing = false;

            std::atomic_bool isValid = true;

        };

    }

}