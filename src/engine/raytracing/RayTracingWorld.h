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

                // The deformation of this frame is recorded with the instances, it needs the global descriptor sets
                std::vector<DeformDispatch> deformDispatches;
                std::vector<DeformBlas> deformBlases;
                Ref<Graphics::Buffer> deformJobBuffer;
                Ref<Graphics::Buffer> deformGroupJobBuffer;
                Ref<Graphics::Buffer> jointMatrixBuffer;

                // The hit records and graph hit groups the instances were written with
                Ref<const std::vector<uint32_t>> hitRecords;
                Ref<const std::vector<MaterialGraphHitGroup>> materialGraphHitGroups;
            };

            RayTracingWorld();

            // The dirty entities were created, moved or changed in this update
            void Update(Scene::Subset<MeshComponent, TransformComponent> subset, const std::vector<ECS::Entity>& dirtyEntities,
                bool updateTriangleLights);

            void UpdateMaterials();

            // The GPU scene writes its hardware ray tracing instances next to the ones prepared by the update and builds
            // the acceleration structure with the methods below. Returns nullptr if the current frame has no update.
            const PendingInstances* GetPendingInstances() const;

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

                std::vector<GPULight> triangleLights;
            };

            // With hardware ray tracing the vertices of nearby animated meshes are deformed into a BLAS of the entity,
            // the positions only exist while it is built and hits read everything but the normals from the mesh
            struct DeformedInstance {
                ResourceHandle<Mesh::Mesh> mesh;
                // Mesh::BuildBVH creates a new BLAS for changed data, which invalidates the deformed one
                Ref<BLAS> meshBlas;
                Ref<Graphics::BLAS> blas;
                Buffer::Buffer normalBuffer;

                // A static mesh of the GPU scene, deformed by wind or a material graph
                bool deformedStatic = false;
                // Further away the data is kept for a while, but the mesh BLAS is traced
                bool active = false;
                bool used = false;
            };

            // The pipeline and graph parameters of a sub mesh, the same for all instances of a mesh
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
                std::vector<SubDataDeformation> deformations;
                Buffer::Buffer vertexBuffer;
                std::vector<std::pair<uint32_t, uint32_t>> subDataVertexRanges;
            };

            // Animated meshes have a BLAS per entity which is skinned on the GPU (software ray tracing)
            struct SkinnedBlas {
                ResourceHandle<Mesh::Mesh> mesh;
                Ref<BLAS> blas;

                Buffer::Buffer triangleVertexBuffer;
                Buffer::Buffer levelNodeBuffer;
                Buffer::Buffer nodeBoundsBuffer;

                // The nodes of each tree level are levelOffsets[i] to levelOffsets[i + 1] in the level node buffer
                std::vector<uint32_t> levelOffsets;

                uint32_t jointOffset = 0;
                bool used = false;
                // Poses that didn't change keep their skinned triangles
                bool skin = false;
            };

            struct alignas(16) SkinPushConstants {
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
                uint32_t padding0 = 0;
                uint32_t padding1 = 0;
                uint32_t padding2 = 0;
            };

            struct JobContext {
                std::vector<GPUBVHInstance> gpuBvhInstances;
                std::vector<Volume::AABB> instanceAABBs;
                std::vector<mat3x4> lastMatrices;
            };

            void UpdateMaterials(std::vector<GPUMaterial>& materials);

            void AllocateHitRecords(BlasInfo& blasInfo, uint32_t count);

            void ReleaseHitRecords(BlasInfo& blasInfo);

            void UpdateForSoftwareRayTracing(vec3 cameraLocation, bool hasCamera);

            void UpdateForHardwareRayTracing(vec3 cameraLocation, bool hasCamera);

            void BuildSoftwareTLAS(std::vector<GPUBVHInstance>& gpuBvhInstances,
                std::vector<mat3x4>& lastMatrices, std::vector<Volume::AABB>& instanceAABBs);

            void UpdateSkinnedBlases();

            void SkinBlases(Graphics::CommandList* commandList);

            void UpdateDeformedInstances(const std::vector<ECS::Entity>& dirtyEntities);

            void RecordDeformation(Graphics::CommandList* commandList);

            DeformedInstance CreateDeformedInstance(const ResourceHandle<Mesh::Mesh>& mesh);

            bool IsDeformedByMaterial(const ResourceHandle<Mesh::Mesh>& mesh) const;

            SkinnedBlas CreateSkinnedBlas(const ResourceHandle<Mesh::Mesh>& mesh);

            void BuildTriangleLightsForMesh(ResourceHandle<Mesh::Mesh>& mesh);

            void UpdateTriangleLights();

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

            std::vector<GPULight> triangleLights;

            std::unordered_map<ECS::Entity, SkinnedBlas> skinnedBlases;
            std::unordered_map<size_t, Buffer::Buffer> skinVertexBuffers;

            std::vector<mat3x4> jointMatrices;
            Buffer::Buffer jointMatricesBuffer;

            std::unordered_map<ECS::Entity, DeformedInstance> deformedInstances;
            std::unordered_map<size_t, DeformMesh> deformMeshes;
            // Meshes of the GPU scene with instances written on the host within the deformation distance
            std::unordered_set<size_t> deformedMeshes;
            // Entities of deformed meshes near the camera, see UpdateDeformedInstances
            std::vector<ECS::Entity> deformCandidates;
            vec3 deformCandidateLocation = vec3(0.0f);
            size_t deformCandidateSlotCount = 0;
            int32_t deformCandidateFrames = 0;
            Ref<Graphics::Buffer> deformPositionBuffer;
            // The deformation of the last update, handed to the pending instances
            std::vector<DeformDispatch> deformDispatches;
            std::vector<DeformBlas> deformBlases;
            Ref<Graphics::Buffer> deformJobUpload;
            Ref<Graphics::Buffer> deformGroupJobUpload;
            Ref<Graphics::Buffer> jointMatrixUpload;
            // Builds while rendering, separate from the scratch memory of the update
            Graphics::ASBuilder deformBlasBuilder;

            std::vector<GPUSkinJob> skinJobs;
            std::vector<uint32_t> skinGroupJobs;
            Buffer::Buffer skinJobBuffer;
            Buffer::Buffer skinGroupJobBuffer;

            PipelineConfig skinTrianglesPipelineConfig;
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
            std::mutex mutex;

        };

    }

}