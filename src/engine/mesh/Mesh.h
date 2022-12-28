#ifndef AE_MESH_H
#define AE_MESH_H

#include "../System.h"
#include "../Material.h"

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

            explicit Mesh(MeshData& meshData, MeshMobility mobility = MeshMobility::Stationary);

            void SetTransform(mat4 transform);

            void UpdateData();

            VkPipelineVertexInputStateCreateInfo GetVertexInputState();

            std::string name = "";

            Graphics::IndexBuffer indexBuffer;
            Graphics::VertexBuffer vertexBuffer;
            Graphics::VertexBuffer normalBuffer;
            Graphics::VertexBuffer tangentBuffer;
            Graphics::VertexBuffer texCoordBuffer;

            MeshData data;
            MeshMobility mobility;

            Impostor* impostor = nullptr;

            bool cullBackFaces = true;
            bool depthTest = true;

            bool castShadow = true;
            bool vegetation = false;

            int32_t allowedShadowCascades = 6;

            float impostorDistance = 300.0f;
            float impostorShadowDistance = 100.0f;

            bool invertUVs = false;

        private:
            std::vector<VkVertexInputBindingDescription> bindingDescriptions;
            std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

        };


	}

}

#endif