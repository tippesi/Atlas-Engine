#include "Mesh.h"
#include "../common/Path.h"

#include "../renderer/OpaqueRenderer.h"
#include "../renderer/ShadowRenderer.h"

namespace Atlas {

    namespace Mesh {

        Mesh::Mesh(ResourceHandle<MeshData> meshData,
            MeshMobility mobility) : data(meshData), mobility(mobility) {

            CheckForLoad();

        }

        void Mesh::SetTransform(mat4 matrix) {

            if (!data.IsLoaded()) {
                data.WaitForLoad();
            }

            data->SetTransform(matrix);

            UpdateData();

        }

        void Mesh::UpdateData() {

            if (data->indices.ContainsData()) {
                auto type = data->indices.GetElementSize() == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
                Buffer::IndexBuffer buffer(type, data->GetIndexCount(), data->indices.GetConvertedVoid());
                vertexArray.AddIndexComponent(buffer);
            }
            if (data->vertices.ContainsData()) {
                Buffer::VertexBuffer buffer(data->vertices.GetFormat(), data->GetVertexCount(),
                    data->vertices.GetConvertedVoid());
                vertexArray.AddComponent(0, buffer);
            }
            if (data->normals.ContainsData()) {
                Buffer::VertexBuffer buffer(data->normals.GetFormat(), data->GetVertexCount(),
                    data->normals.GetConvertedVoid());
                vertexArray.AddComponent(1, buffer);
            }
            if (data->texCoords.ContainsData()) {
                Buffer::VertexBuffer buffer(data->texCoords.GetFormat(), data->GetVertexCount(),
                    data->texCoords.GetConvertedVoid());
                vertexArray.AddComponent(2, buffer);
            }
            if (data->tangents.ContainsData()) {
                Buffer::VertexBuffer buffer(data->tangents.GetFormat(), data->GetVertexCount(),
                    data->tangents.GetConvertedVoid());
                vertexArray.AddComponent(3, buffer);
            }

        }

        bool Mesh::CheckForLoad() {

            if (data.IsLoaded() && !isLoaded) {
                isLoaded = true;

                UpdateData();
            }

            return isLoaded;

        }

    }

}