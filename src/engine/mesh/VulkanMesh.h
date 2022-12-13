#ifndef AE_VULKANMESH_H
#define AE_VULKANMESH_H

#include "../System.h"
#include "../Material.h"

#include "MeshData.h"

#include "../graphics/Buffer.h"

namespace Atlas {

    namespace Mesh {

        struct VulkanMeshSubData {

            std::string name;

            uint32_t indicesOffset;
            uint32_t indicesCount;

            Material* material;

            Volume::AABB aabb;

        };

        class VulkanMesh {

        public:
            VulkanMesh(MeshData& meshData);

            Graphics::IndexBuffer indexBuffer;
            Graphics::VertexBuffer vertexBuffer;
            Graphics::VertexBuffer normalBuffer;
            Graphics::VertexBuffer tangentBuffer;
            Graphics::VertexBuffer texCoordBuffer;

            VkPipelineVertexInputStateCreateInfo GetVertexInputState();

            MeshData data;

        private:
            void UploadData(MeshData& data);

            std::vector<VkVertexInputBindingDescription> bindingDescriptions;
            std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

        };

    }

}

#endif
