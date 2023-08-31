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
            }
            if (data.vertices.ContainsData()) {
                vertexBuffer = Buffer::VertexBuffer(data.vertices.GetFormat(), data.GetVertexCount(),
                    data.vertices.GetConvertedVoid(), hostAccessible);
            }
            if (data.normals.ContainsData()) {
                normalBuffer = Buffer::VertexBuffer(data.normals.GetFormat(), data.GetVertexCount(),
                    data.normals.GetConvertedVoid(), hostAccessible);
            }
            if (data.texCoords.ContainsData()) {
                texCoordBuffer = Buffer::VertexBuffer(data.texCoords.GetFormat(), data.GetVertexCount(),
                    data.texCoords.GetConvertedVoid(), hostAccessible);
            }
            if (data.tangents.ContainsData()) {
                tangentBuffer = Buffer::VertexBuffer(data.tangents.GetFormat(), data.GetVertexCount(),
                    data.tangents.GetConvertedVoid(), hostAccessible);
            }
            if (data.colors.ContainsData()) {
                colorBuffer = Buffer::VertexBuffer(data.colors.GetFormat(), data.GetVertexCount(),
                    data.colors.GetConvertedVoid(), hostAccessible);
            }

            UpdateVertexArray();

        }

        void Mesh::UpdateVertexArray() {

            vertexArray.AddIndexComponent(indexBuffer);
            vertexArray.AddComponent(0, vertexBuffer);
            if (data.normals.ContainsData())
                vertexArray.AddComponent(1, normalBuffer);
            if (data.texCoords.ContainsData())
                vertexArray.AddComponent(2, texCoordBuffer);
            if (data.tangents.ContainsData())
                vertexArray.AddComponent(3, tangentBuffer);
            if (data.colors.ContainsData())
                vertexArray.AddComponent(4, colorBuffer);

        }

    }

}