#include "Mesh.h"
#include "../common/Path.h"

#include "../renderer/OpaqueRenderer.h"
#include "../renderer/ShadowRenderer.h"

namespace Atlas {

	namespace Mesh {

        Mesh::Mesh(MeshData &meshData, MeshMobility mobility) : data(meshData), mobility(mobility) {

            UpdateData();

        }

        void Mesh::SetTransform(mat4 matrix) {

            data.SetTransform(matrix);

            UpdateData();

        }

        void Mesh::UpdateData() {

            auto graphicsDevice = Graphics::GraphicsDevice::DefaultDevice;

            if (data.indices.ContainsData()) {
                Graphics::BufferDesc desc {
                    .usageFlags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    .data = data.indices.GetConvertedVoid(),
                    .size = data.indices.GetElementSize() * data.GetIndexCount()
                };
                indexBuffer.buffer = graphicsDevice->CreateBuffer(desc);
                indexBuffer.type = data.indices.GetElementSize() == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
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
                    data.vertices.GetFormat());
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
                    data.normals.GetFormat());
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
                    data.texCoords.GetFormat());
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
                    data.tangents.GetFormat());
            }

        }

        VkPipelineVertexInputStateCreateInfo Mesh::GetVertexInputState() {

            bindingDescriptions.clear();
            attributeDescriptions.clear();

            if (vertexBuffer.buffer) bindingDescriptions.push_back(vertexBuffer.bindingDescription);
            if (normalBuffer.buffer) bindingDescriptions.push_back(normalBuffer.bindingDescription);
            if (tangentBuffer.buffer) bindingDescriptions.push_back(tangentBuffer.bindingDescription);
            if (texCoordBuffer.buffer) bindingDescriptions.push_back(texCoordBuffer.bindingDescription);

            if (vertexBuffer.buffer) attributeDescriptions.push_back(vertexBuffer.attributeDescription);
            if (normalBuffer.buffer) attributeDescriptions.push_back(normalBuffer.attributeDescription);
            if (tangentBuffer.buffer) attributeDescriptions.push_back(tangentBuffer.attributeDescription);
            if (texCoordBuffer.buffer) attributeDescriptions.push_back(texCoordBuffer.attributeDescription);

            return Graphics::Initializers::InitPipelineVertexInputStateCreateInfo(
                bindingDescriptions.data(), uint32_t(bindingDescriptions.size()),
                attributeDescriptions.data(), uint32_t(attributeDescriptions.size())
            );

        }

	}

}