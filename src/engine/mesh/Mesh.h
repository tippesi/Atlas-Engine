#pragma once

#include "../System.h"
#include "../Material.h"
#include "resource/Resource.h"
#include "../buffer/VertexArray.h"

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

        typedef uint32_t MeshUsage;

        typedef enum MeshUsageBits {
            MultiBufferedBit = (1 << 0),
            HostAccessBit = (1 << 1),
        } MeshUsageBits;

        class Mesh {

            friend RayTracing::RayTracingWorld;

        public:
            Mesh() = default;

            explicit Mesh(MeshData& meshData, MeshMobility mobility = MeshMobility::Stationary,
                MeshUsage usage = 0);

            explicit Mesh(MeshMobility mobility, MeshUsage usage = 0);

            /**
             * Fully updates the buffer data with data available through the MeshData member
             */
            void UpdateData();

            /*
             * Resets material pipeline configs 
             */
            void UpdateMaterials();

            /**
             * Updates the vertex array based on the state of the vertex buffers.
             * @note This is useful when running your own data pipeline
             */
            void UpdateVertexArray();

            /**
             * Builds up BVH and fills raytracing related buffers
             */
            void BuildBVH(bool parallelBuild = true);


            bool IsBVHBuilt() const;

            std::string name = "";

            MeshData data;
            MeshMobility mobility = MeshMobility::Stationary;
            MeshUsage usage = 0;

            Buffer::VertexArray vertexArray;

            Buffer::IndexBuffer indexBuffer;
            Buffer::VertexBuffer vertexBuffer;
            Buffer::VertexBuffer normalBuffer;
            Buffer::VertexBuffer texCoordBuffer;
            Buffer::VertexBuffer tangentBuffer;
            Buffer::VertexBuffer colorBuffer;

            Buffer::Buffer blasNodeBuffer;
            Buffer::Buffer triangleBuffer;
            Buffer::Buffer bvhTriangleBuffer;
            Buffer::Buffer triangleOffsetBuffer;

            Ref<Graphics::BLAS> blas = nullptr;

            Ref<Impostor> impostor = nullptr;

            bool cullBackFaces = true;
            bool depthTest = true;

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

        private:
            bool isLoaded = false;

            std::atomic_bool isBvhBuilt = false;
            std::atomic_bool needsBvhRefresh = false;

        };


    }

}