#pragma once

#include "System.h"

#include "RTStructures.h"
#include "BLAS.h"
#include "scene/Subset.h"

#include "scene/components/MeshComponent.h"
#include "scene/components/TransformComponent.h"

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

    }

    namespace RayTracing {

        class RayTracingWorld {

            friend Renderer::Helper::RayTracingHelper;
            friend Scene::Scene;

        public:
            RayTracingWorld();

            void Update(Scene::Subset<MeshComponent, TransformComponent> subset, bool updateTriangleLights);

            void UpdateMaterials();

            bool IsValid();

            void Clear();

            bool includeObjectHistory = false;

        private:
            struct BlasInfo {
                ResourceHandle<Mesh::Mesh> mesh;

                int32_t offset = 0;
                int32_t materialOffset = 0;

                float cullingDistanceSqr = 0.0f;

                int32_t idx = 0;

                std::vector<GPULight> triangleLights;
                std::vector<uint32_t> instanceIndices;
                std::vector<mat4x3> matrices;
            };

            void UpdateMaterials(std::vector<GPUMaterial>& materials);

            void UpdateForSoftwareRayTracing(std::vector<GPUBVHInstance>& gpuBvhInstances,
                std::vector<mat3x4>& lastMatrices, std::vector<Volume::AABB>& instanceAABBs);

            void UpdateForHardwareRayTracing(Scene::Subset<MeshComponent,
                TransformComponent>& entitySubset, size_t instanceCount);

            void BuildTriangleLightsForMesh(ResourceHandle<Mesh::Mesh>& mesh);

            void UpdateTriangleLights();

            Scene::Scene* scene;

            Ref<Graphics::TLAS> tlas;

            std::vector<Ref<BLAS>> blases;
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

            std::unordered_map<Ref<BLAS>, BlasInfo> blasInfos;
            std::unordered_map<Ref<BLAS>, BlasInfo> prevBlasInfos;

            std::vector<GPUMaterial> materials;

            bool hardwareRayTracing = false;

            std::atomic_bool isValid = true;
            std::mutex mutex;

        };

    }

}