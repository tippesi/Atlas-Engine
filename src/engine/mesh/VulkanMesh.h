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

        struct VertexBuffer {
            Graphics::Buffer* buffer = nullptr;
            VkVertexInputBindingDescription bindingDescription = {};
            VkVertexInputAttributeDescription attributeDescription = {};
        };

        class VulkanMesh {

        public:
            VulkanMesh(MeshData& meshData);

            Graphics::Buffer* indexBuffer;
            VertexBuffer vertexBuffer;
            VertexBuffer normalBuffer;
            VertexBuffer tangentBuffer;
            VertexBuffer texCoordBuffer;

        private:
            void UploadData(MeshData& data);

        };

    }

}

#endif
