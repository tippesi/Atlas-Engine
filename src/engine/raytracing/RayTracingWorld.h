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

#include "texture/TextureAtlas.h"

#include <vector>
#include <optional>
#include <unordered_map>

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
            };

            RayTracingWorld();

            void Update(Scene::Subset<MeshComponent, TransformComponent> subset, bool updateTriangleLights);

            void UpdateMaterials();

            // The GPU scene writes its hardware ray tracing instances next to the ones prepared by the update and builds
            // the acceleration structure with the methods below. Returns nullptr if the current frame has no update.
            const PendingInstances* GetPendingInstances() const;

            void PrepareInstances(Graphics::CommandList* commandList);

            void BindInstanceOutputs(Graphics::CommandList* commandList);

            void FinishInstances(Graphics::CommandList* commandList);

            bool IsValid();

            void Clear();

            bool includeObjectHistory = false;

            // Hardware ray tracing refits the BLASes of animated meshes instead of building them every frame.
            // Software ray tracing always refits them.
            bool refitSkinnedBlases = false;

            // Animated meshes further away from the camera are traced in their rest pose, skinning and building
            // a BLAS for every animated entity of a large scene each frame is too expensive
            float skinnedBlasDistance = 60.0f;

        private:
            struct BlasInfo {
                ResourceHandle<Mesh::Mesh> mesh;
                Terrain::TerrainNode* node = nullptr;

                int32_t offset = 0;
                int32_t materialOffset = 0;

                float cullingDistanceSqr = 0.0f;

                int32_t idx = 0;

                std::vector<GPULight> triangleLights;
            };

            // Animated meshes have a BLAS per entity which is skinned on the GPU
            struct SkinnedBlas {
                ResourceHandle<Mesh::Mesh> mesh;
                Ref<BLAS> blas;

                Buffer::Buffer triangleVertexBuffer;
                Buffer::Buffer levelNodeBuffer;
                Buffer::Buffer nodeBoundsBuffer;
                Buffer::VertexBuffer vertexBuffer;

                // The nodes of each tree level are levelOffsets[i] to levelOffsets[i + 1] in the level node buffer
                std::vector<uint32_t> levelOffsets;

                uint32_t jointOffset = 0;
                bool used = false;
            };

            struct alignas(16) SkinPushConstants {
                uint32_t elementCount;
                uint32_t jointOffset;
            };

            struct alignas(16) RefitPushConstants {
                uint32_t nodeOffset;
                uint32_t nodeCount;
            };

            // Has to match RT_MESH_USABLE in culling/rtinstances.hsh
            static constexpr uint32_t rtMeshUsable = 1;

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
                uint32_t padding0 = 0;
                VkDeviceAddress blasAddress = 0;
            };

            struct JobContext {
                std::vector<GPUBVHInstance> gpuBvhInstances;
                std::vector<Volume::AABB> instanceAABBs;
                std::vector<mat3x4> lastMatrices;
            };

            void UpdateMaterials(std::vector<GPUMaterial>& materials);

            void UpdateForSoftwareRayTracing(vec3 cameraLocation, bool hasCamera);

            void UpdateForHardwareRayTracing(vec3 cameraLocation, bool hasCamera);

            void BuildSoftwareTLAS(std::vector<GPUBVHInstance>& gpuBvhInstances,
                std::vector<mat3x4>& lastMatrices, std::vector<Volume::AABB>& instanceAABBs);

            void UpdateSkinnedBlases(int32_t& meshCount);

            void SkinBlases(Graphics::CommandList* commandList);

            SkinnedBlas CreateSkinnedBlas(const ResourceHandle<Mesh::Mesh>& mesh);

            void BuildTriangleLightsForMesh(ResourceHandle<Mesh::Mesh>& mesh);

            void UpdateTriangleLights();

            Scene::Scene* scene;

            Graphics::ASBuilder tlasBuilder;
            Ref<Graphics::TLAS> tlas;

            std::vector<Ref<Graphics::BLAS>> buildBlases;
            std::vector<Ref<Graphics::BLAS>> refitBlases;

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

            PipelineConfig skinTrianglesPipelineConfig;
            PipelineConfig skinVerticesPipelineConfig;
            PipelineConfig refitBVHPipelineConfig;

            // Hardware instances are written on the GPU, the ones of the host first
            std::vector<GPURTMesh> rtMeshes;
            std::optional<PendingInstances> pendingInstances;
            Ref<Graphics::Buffer> tlasInstanceBuffer;
            Ref<Graphics::Buffer> instanceCounterBuffer;
            Ref<Graphics::MultiBuffer> instanceCountReadback;
            // The readback holds a count once every frame slot was written
            uint32_t readbackFrameCount = 0;
            Graphics::UploadBufferPool uploadBufferPool;

            std::unordered_map<Ref<BLAS>, BlasInfo> blasInfos;
            std::unordered_map<Ref<BLAS>, BlasInfo> prevBlasInfos;

            std::vector<GPUMaterial> materials;
            std::vector<JobContext> jobContexts;

            bool hardwareRayTracing = false;

            std::atomic_bool isValid = true;
            std::mutex mutex;

        };

    }

}