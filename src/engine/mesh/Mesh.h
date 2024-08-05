#pragma once

#include "../System.h"
#include "../Material.h"
#include "resource/Resource.h"

#include "MeshData.h"
#include "Impostor.h"

#include "../graphics/Buffer.h"
#include "../graphics/BLAS.h"

namespace Atlas {

    namespace RayTracing {
        class RayTracingWorld;
    }

    namespace Mesh {

        enum class MeshMobility {
            Stationary = 0,
            Movable
        };

        class Mesh {

            friend RayTracing::RayTracingWorld;

        public:
            Mesh() = default;

            explicit Mesh(const ResourceHandle<MeshData>& meshData, std::vector<uint32_t> subDataIndices = {},
                MeshMobility mobility = MeshMobility::Stationary);

            explicit Mesh(MeshMobility mobility);

            /*
             * Resets material pipeline configs 
             */
            void UpdateMaterials();

            void Update();

            /**
             * Builds up BVH and fills raytracing related buffers
             */
            void BuildBVH(bool parallelBuild = true);


            bool IsBVHBuilt() const;

            std::string name = "";

            ResourceHandle<MeshData> data;
            MeshMobility mobility = MeshMobility::Stationary;

            std::vector<uint32_t> subDataIndices;

            Buffer::Buffer blasNodeBuffer;
            Buffer::Buffer triangleBuffer;
            Buffer::Buffer bvhTriangleBuffer;
            Buffer::Buffer triangleOffsetBuffer;

            Ref<Graphics::BLAS> blas = nullptr;

            Ref<Impostor> impostor = nullptr;

            Volume::AABB aabb;
            float radius;

            bool cullBackFaces = true;
            bool castShadow = true;

            bool vegetation = false;
            float windNoiseTextureLod = 2.0f;
            float windBendScale = 1.0f;
            float windWiggleScale = 1.0f;

            int32_t allowedShadowCascades = 6;

            float distanceCulling = 10000.0f;
            float shadowDistanceCulling = 10000.0f;
            float impostorDistance = 300.0f;
            float impostorShadowDistance = 100.0f;

            bool invertUVs = false;
            bool compactedData = true;

        private:
            void BuildBVHData(bool parallelBuild);

            std::atomic_bool isBvhBuilt = false;
            std::atomic_bool needsBvhRefresh = false;

            std::vector<GPUTriangle> gpuTriangles;
            std::vector<GPUBVHTriangle> gpuBvhTriangles;
            std::vector<GPUBVHNode> gpuBvhNodes;

        };


    }

}