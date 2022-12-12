#include "VulkanMesh.h"
#include "../EngineInstance.h"

namespace Atlas {

    namespace Mesh {

        VulkanMesh::VulkanMesh(Atlas::Mesh::MeshData &meshData) {



        }

        void VulkanMesh::UploadData(Atlas::Mesh::MeshData &data) {

            auto graphicsInstance = EngineInstance::GetGraphicsInstance();
            auto graphicsDevice = graphicsInstance->GetGraphicsDevice();

            if (data.indices.ContainsData()) {
                Graphics::BufferDesc desc {
                  .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  .data = data.indices.GetConvertedVoid(),
                  .size = data.indices.GetElementSize() * data.GetIndexCount()
                };
                indexBuffer = graphicsDevice->CreateBuffer(desc);
            }
            if (data.vertices.ContainsData()) {
                Graphics::BufferDesc desc {
                    .usageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    .data = data.vertices.GetConvertedVoid(),
                    .size = data.vertices.GetElementSize() * data.GetVertexCount()
                };
                vertexBuffer.buffer = graphicsDevice->CreateBuffer(desc);
                vertexBuffer.bindingDescription = Graphics::Initializers::InitVertexInputBindingDescription(0,
                    data.vertices.GetElementSize());
                vertexBuffer.attributeDescription = Graphics::Initializers::InitVertexInputAttributeDescription(0,
                    VK_FORMAT_R32G32B32_SFLOAT);
            }
            if (data.normals.ContainsData()) {
                Graphics::BufferDesc desc {
                    .usageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    .data = data.normals.GetConvertedVoid(),
                    .size = data.normals.GetElementSize() * data.GetVertexCount()
                };
                normalBuffer.buffer = graphicsDevice->CreateBuffer(desc);
                normalBuffer.bindingDescription = Graphics::Initializers::InitVertexInputBindingDescription(1,
                    data.normals.GetElementSize());
                normalBuffer.attributeDescription = Graphics::Initializers::InitVertexInputAttributeDescription(1,
                    VK_FORMAT_R32G32B32_SFLOAT);
            }
            if (data.texCoords.ContainsData()) {
                Graphics::BufferDesc desc {
                    .usageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    .data = data.texCoords.GetConvertedVoid(),
                    .size = data.texCoords.GetElementSize() * data.GetVertexCount()
                };
                texCoordBuffer.buffer = graphicsDevice->CreateBuffer(desc);
                texCoordBuffer.bindingDescription = Graphics::Initializers::InitVertexInputBindingDescription(2,
                    data.texCoords.GetElementSize());
                texCoordBuffer.attributeDescription = Graphics::Initializers::InitVertexInputAttributeDescription(2,
                    VK_FORMAT_R32G32B32_SFLOAT);
            }
            if (data.tangents.ContainsData()) {
                Graphics::BufferDesc desc {
                    .usageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    .data = data.tangents.GetConvertedVoid(),
                    .size = data.tangents.GetElementSize() * data.GetVertexCount()
                };
                tangentBuffer.buffer = graphicsDevice->CreateBuffer(desc);
                tangentBuffer.bindingDescription = Graphics::Initializers::InitVertexInputBindingDescription(3,
                    data.tangents.GetElementSize());
                tangentBuffer.attributeDescription = Graphics::Initializers::InitVertexInputAttributeDescription(3,
                    VK_FORMAT_R32G32B32_SFLOAT);
            }

        }

    }

}