#ifndef AE_MESH_H
#define AE_MESH_H

#include "../System.h"
#include "../Material.h"
#include "resource/Resource.h"
#include "../buffer/VertexArray.h"

#include "MeshData.h"
#include "Impostor.h"

#include "../graphics/Buffer.h"

namespace Atlas {

    namespace Mesh {

        enum class MeshMobility {
            Stationary = 0,
            Movable
        };

        class Mesh {

        public:
            Mesh() = default;

            explicit Mesh(ResourceHandle<MeshData> meshData,
                MeshMobility mobility = MeshMobility::Stationary);

            void SetTransform(mat4 transform);

            void UpdateData();

            std::string name = "";

            ResourceHandle<MeshData> data;
            MeshMobility mobility = MeshMobility::Stationary;
            Buffer::VertexArray vertexArray;

            Impostor* impostor = nullptr;

            bool cullBackFaces = true;
            bool depthTest = true;

            bool castShadow = true;
            bool vegetation = false;

            int32_t allowedShadowCascades = 6;

            float impostorDistance = 300.0f;
            float impostorShadowDistance = 100.0f;

            bool invertUVs = false;

        };


    }

}

#endif