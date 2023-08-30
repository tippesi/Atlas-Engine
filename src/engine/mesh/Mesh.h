#ifndef AE_MESH_H
#define AE_MESH_H

#include "../System.h"
#include "../Material.h"
#include "resource/Resource.h"
#include "../buffer/VertexArray.h"

#include "MeshData.h"
#include "Impostor.h"

#include "../graphics/Buffer.h"
#include "../graphics/BLAS.h"

namespace Atlas {

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

        public:
            Mesh() = default;

            explicit Mesh(MeshData& meshData, MeshMobility mobility = MeshMobility::Stationary,
                MeshUsage usage = 0);

            explicit Mesh(MeshMobility mobility, MeshUsage usage = 0);

            void SetTransform(mat4 transform);

            void UpdateData();

            std::string name = "";

            MeshData data;
            MeshMobility mobility = MeshMobility::Stationary;
            MeshUsage usage = 0;

            Buffer::VertexArray vertexArray;
            Buffer::IndexBuffer indexBuffer;
            Buffer::VertexBuffer vertexBuffer;

            Ref<Graphics::BLAS> blas = nullptr;

            Ref<Impostor> impostor = nullptr;

            bool cullBackFaces = true;
            bool depthTest = true;

            bool castShadow = true;
            bool vegetation = false;

            int32_t allowedShadowCascades = 6;

            float impostorDistance = 300.0f;
            float impostorShadowDistance = 100.0f;

            bool invertUVs = false;

        private:
            bool isLoaded = false;

        };


    }

}

#endif