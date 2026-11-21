#pragma once

#include "../System.h"
#include "../Material.h"
#include "resource/Resource.h"
#include "../buffer/VertexArray.h"
#include "../buffer/GeometryPool.h"
#include "../raytracing/BLAS.h"

#include "MeshData.h"
#include "Impostor.h"
#include "SoftBodySimulationMesh.h"

#include "../graphics/Buffer.h"
#include "../graphics/BLAS.h"
#include "../jobsystem/JobGroup.h"

#include <mutex>

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

            ~Mesh();

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
             * Generates the levels of detail of the data in a job, ApplyGeneratedLods takes them over
             */
            void GenerateLods(const MeshLodSettings& settings = {});

            /**
             * Removes the levels of detail like GenerateLods, ApplyGeneratedLods takes the change over
             */
            void ClearLods();

            bool IsGeneratingLods() const;

            /**
             * Waits until the levels of detail of this mesh are generated, ApplyGeneratedLods still takes them over
             */
            void WaitForLods();

            /**
             * Takes over the levels of detail which are generated and updates the buffers. Has to be called while nothing
             * renders or reads meshes, the scene update does it after the streaming
             * @param wait Waits for builds of buffers or acceleration structures of a mesh instead of taking its levels
             * over in a later call
             */
            static void ApplyGeneratedLods(bool wait = false);

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

            // Changes whenever the buffers are created or released
            uint32_t GetGeometryVersion() const { return geometryVersion; }

            // Users of the buffers beyond drawing keep the range, it stays reserved for them
            const Ref<Buffer::GeometryPool::Range>& GetGeometryRange() const { return geometryRange; }

            // Simulated soft bodies draw level 0 of the mesh, which stays resident while there are any
            bool HasSoftBodies() const { return softBodyCount > 0; }

            // Called by soft bodies when they are created or removed
            void ChangeSoftBodyCount(int32_t change);

            // The mesh soft bodies simulate, created from level 0 and the soft body settings on first use. Thread safe,
            // soft bodies create it in a job
            Ref<SoftBodySimulationMesh> GetSoftBodySimulationMesh();

            // Whether the simulation mesh is the current one, doesn't create it
            bool IsSoftBodySimulationMesh(const Ref<SoftBodySimulationMesh>& simulationMesh);

            // Creates the simulation mesh again on its next use, bodies with the old one are created again
            void ResetSoftBodySimulationMesh();

            // Bytes of the vertex and index buffers
            size_t GetGeometrySize() const;

            /**
             * The level of detail of an instance, like the GPU culling chooses it
             * @param globalMatrix The matrix of the instance
             * @param bounds The bounds of the instance
             * @param cameraLocation Location of the main camera
             * @param focalLength Viewport height in pixels divided by 2 * tan(fov / 2)
             * @param errorScale Scales the pixel error, see Scene::meshLodErrorScale
             * @param forcedLod Used instead if it isn't negative, see Scene::forcedMeshLod
             */
            int32_t SelectLod(const mat4& globalMatrix, const Volume::AABB& bounds, vec3 cameraLocation,
                float focalLength, float errorScale, int32_t forcedLod) const;

            std::string name = "";

            MeshData data;
            MeshMobility mobility = MeshMobility::Stationary;
            MeshUsage usage = 0;

            Buffer::VertexArray vertexArray;

            Buffer::IndexBuffer indexBuffer;
            Buffer::VertexBuffer vertexBuffer;
            Buffer::VertexBuffer normalBuffer;
            Buffer::VertexBuffer texCoordBuffer;
            Buffer::VertexBuffer texCoord1Buffer;
            Buffer::VertexBuffer tangentBuffer;
            Buffer::VertexBuffer colorBuffer;
            Buffer::VertexBuffer jointBuffer;
            Buffer::VertexBuffer weightBuffer;
            // Rest pose vertices of skinned meshes for the hardware ray tracing BLAS
            Buffer::VertexBuffer restVertexBuffer;

            Ref<RayTracing::BLAS> blas = nullptr;
            // The levels of detail from 1 on, replaced together with the BLAS of level 0
            std::vector<Ref<RayTracing::BLAS>> lodBlases;
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
            // Instances use the coarsest level of detail whose geometric error stays below this many pixels of the
            // output, scaled by Scene::meshLodErrorScale
            float lodPixelError = 2.0f;
            // How far graphs of the materials can move vertices. The GPU scene reads it for every mesh in
            // every frame, like the culling values above
            float renderBoundsPadding = 0.0f;

            // Animations of skinned meshes update every frame up to this distance from the camera and one frame less
            // often per further multiple of it, out of view half as often. Zero updates them every frame
            float animationLodDistance = 20.0f;
            // The most frames between two animation updates
            int32_t animationMaxInterval = 4;

            bool invertUVs = false;

            SoftBodyMeshSettings softBody;

        private:
            void CreatePooledBuffers();

            bool isLoaded = false;
            std::atomic_bool blasCreated = false;
            std::atomic_bool geometryResident = true;
            std::atomic_uint32_t geometryVersion = 0;
            // The vertex and index buffers of the mesh, host accessible meshes have buffers of their own
            Ref<Buffer::GeometryPool::Range> geometryRange;
            std::atomic_int32_t softBodyCount = 0;
            Ref<SoftBodySimulationMesh> softBodySimulationMesh;
            std::mutex softBodySimulationMeshMutex;

            // Guards the data against taking over levels of detail while buffers or acceleration structures are built
            std::mutex dataMutex;

            JobGroup lodJob{ "Mesh levels of detail" };
            Ref<MeshData> generatedLods;
            std::atomic_bool generatingLods = false;

            // Meshes whose levels of detail are generated, until they are taken over
            static std::mutex generatingMutex;
            static std::vector<Mesh*> generatingMeshes;

        };


    }

}
