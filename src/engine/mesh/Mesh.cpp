#include "Mesh.h"
#include "../common/Path.h"

#include "../renderer/OpaqueRenderer.h"
#include "../renderer/ShadowRenderer.h"

namespace Atlas {

    namespace Mesh {

        Mesh::Mesh(MeshData& meshData, MeshMobility mobility, MeshUsage usage)
            : data(meshData), mobility(mobility), usage(usage) {

            UpdateData();

        }

        Mesh::Mesh(MeshMobility mobility, MeshUsage usage)
            : mobility(mobility), usage(usage) {


        }

        void Mesh::SetTransform(mat4 matrix) {

            data.SetTransform(matrix);

            UpdateData();

        }

        void Mesh::UpdateData() {

            bool hostAccessible = usage & MeshUsageBits::HostAccessBit;

            if (data.indices.ContainsData()) {
                auto type = data.indices.GetElementSize() == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
                indexBuffer = Buffer::IndexBuffer(type, data.GetIndexCount(),
                    data.indices.GetConvertedVoid(), hostAccessible);
                vertexArray.AddIndexComponent(indexBuffer);
            }
            if (data.vertices.ContainsData()) {
                vertexBuffer = Buffer::VertexBuffer(data.vertices.GetFormat(), data.GetVertexCount(),
                    data.vertices.GetConvertedVoid(), hostAccessible);
                vertexArray.AddComponent(0, vertexBuffer);
            }
            if (data.normals.ContainsData()) {
                Buffer::VertexBuffer buffer(data.normals.GetFormat(), data.GetVertexCount(),
                    data.normals.GetConvertedVoid(), hostAccessible);
                vertexArray.AddComponent(1, buffer);
            }
            if (data.texCoords.ContainsData()) {
                Buffer::VertexBuffer buffer(data.texCoords.GetFormat(), data.GetVertexCount(),
                    data.texCoords.GetConvertedVoid(), hostAccessible);
                vertexArray.AddComponent(2, buffer);
            }
            if (data.tangents.ContainsData()) {
                Buffer::VertexBuffer buffer(data.tangents.GetFormat(), data.GetVertexCount(),
                    data.tangents.GetConvertedVoid(), hostAccessible);
                vertexArray.AddComponent(3, buffer);
            }
            if (data.colors.ContainsData()) {
                Buffer::VertexBuffer buffer(data.colors.GetFormat(), data.GetVertexCount(),
                    data.colors.GetConvertedVoid(), hostAccessible);
                vertexArray.AddComponent(4, buffer);
            }

        }

    }

}