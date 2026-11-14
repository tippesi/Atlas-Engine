#pragma once

#include "../System.h"
#include "../Material.h"
#include "resource/Resource.h"
#include "../buffer/VertexArray.h"
#include "../raytracing/BLAS.h"

#include "MeshData.h"
#include "Impostor.h"

#include "../graphics/Buffer.h"
#include "../graphics/BLAS.h"

namespace Atlas {

    namespace RayTracing {
        class RayTracingWorld;
    }

    namespace Streaming {
        class MeshStreaming;
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
            friend Streaming::MeshStreaming;

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
            void UpdatePipelines();

            /**
             * The bounds for culling: The bounds of the mesh with the room graphs of its materials can move vertices.
             */
            Volume::AABB GetRenderBounds() const;

            /**
             * Updates the room of the bounds from the graphs of the materials
             */
            void UpdateRenderBounds();

            /**
             * Updates the vertex array based on the state of the vertex buffers.
             * @note This is useful when running your own data pipeline
             */
            void UpdateVertexArray();

            void InvertNormals();

            /**
             * Builds up BVH and fills raytracing related buffers
             */
            void BuildBVH(bool parallelBuild = true);

            void ClearBVH();

            bool IsBVHBuilt() const;

            /**
             * Creates the GPU buffers and the vertex array from the data
             */
            void CreateBuffers();

            /**
             * Releases the GPU buffers and the acceleration structure, the data stays on the host to create them again.
             * Renderers skip meshes without resident geometry, their impostors are drawn instead.
             */
            void UnloadGeometry();

            bool IsGeometryResident() const;

            // Bytes of the vertex and index buffers
            size_t GetGeometrySize() const;

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
            Buffer::VertexBuffer jointBuffer;
            Buffer::VertexBuffer weightBuffer;
            // Rest pose vertices of skinned meshes for the hardware ray tracing BLAS
            Buffer::VertexBuffer restVertexBuffer;

            Ref<RayTracing::BLAS> blas = nullptr;
            ResourceHandle<Impostor> impostor;

            bool cullBackFaces = true;
            bool depthTest = true;
            bool rayTrace = true;

            bool castShadow = true;

            bool vegetation = false;
            float windNoiseTextureLod = 2.0f;
            float windBendScale = 1.0f;
            float windWiggleScale = 1.0f;

            int32_t allowedShadowCascades = 6;

            float distanceCulling = 10000.0f;
            float shadowDistanceCulling = 10000.0f;
            float rayTraceDistanceCulling = 10000.0f;
            float impostorDistance = 300.0f;
            float impostorShadowDistance = 100.0f;
            // Instances with a smaller bounding sphere in pixels of the output are culled, the streaming unloads meshes
            // without bigger instances. Zero disables it
            float minScreenSize = 8.0f;
            // How far graphs of the materials can move vertices. The GPU scene reads it for every mesh in
            // every frame, like the culling values above
            float renderBoundsPadding = 0.0f;

            // Animations of skinned meshes update every frame up to this distance from the camera and one frame less
            // often per further multiple of it, out of view half as often. Zero updates them every frame
            float animationLodDistance = 20.0f;
            // The most frames between two animation updates
            int32_t animationMaxInterval = 4;

            bool invertUVs = false;

        private:
            bool isLoaded = false;
            std::atomic_bool blasCreated = false;
            std::atomic_bool geometryResident = true;

        };


    }

}
