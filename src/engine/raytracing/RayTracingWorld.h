#pragma once

#include "System.h"

#include "RTStructures.h"
#include "BLAS.h"
#include "scene/Subset.h"


#include "terrain/TerrainNode.h"
#include "scene/components/MeshComponent.h"
#include "scene/components/TransformComponent.h"
#include "buffer/VertexBuffer.h"
#include "pipeline/PipelineConfig.h"

#include "texture/TextureAtlas.h"

#include <vector>
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
            RayTracingWorld();

            void Update(Scene::Subset<MeshComponent, TransformComponent> subset, bool updateTriangleLights);

            void UpdateMaterials();

            bool IsValid();

            void Clear();

            bool includeObjectHistory = false;

            // Hardware ray tracing refits the BLASes of animated meshes instead of building them every frame.
            // Software ray tracing always refits them.
            bool refitSkinnedBlases = false;

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

            struct JobContext {
                std::vector<VkAccelerationStructureInstanceKHR> hardwareInstances;
                std::vector<GPUBVHInstance> gpuBvhInstances;
                std::vector<Volume::AABB> instanceAABBs;
                std::vector<mat3x4> lastMatrices;
            };

            void UpdateMaterials(std::vector<GPUMaterial>& materials);

            void UpdateForSoftwareRayTracing(std::vector<GPUBVHInstance>& gpuBvhInstances,
                std::vector<mat3x4>& lastMatrices, std::vector<Volume::AABB>& instanceAABBs);

            void UpdateForHardwareRayTracing(Scene::Subset<MeshComponent,
                TransformComponent>& entitySubset, size_t instanceCount);

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