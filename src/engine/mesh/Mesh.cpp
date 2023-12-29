#include "Mesh.h"
#include "../common/Path.h"

#include "../renderer/OpaqueRenderer.h"
#include "../renderer/ShadowRenderer.h"

#include "../graphics/ASBuilder.h"

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

        void Mesh::BuildBVH(bool parallelBuild) {

            auto device = Graphics::GraphicsDevice::DefaultDevice;
            bool hardwareRayTracing = device->support.hardwareRayTracing;
            bool bindless = device->support.bindless;

            AE_ASSERT(data.indexCount > 0 && "There is no data in this mesh");

            if (data.indexCount == 0 || !bindless) return;

            data.BuildBVH(parallelBuild);

            triangleBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(GPUTriangle));
            triangleBuffer.SetSize(data.gpuTriangles.size());
            triangleBuffer.SetData(data.gpuTriangles.data(), 0, data.gpuTriangles.size());

            if (!hardwareRayTracing) {
                blasNodeBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(GPUBVHNode));
                blasNodeBuffer.SetSize(data.gpuBvhNodes.size());
                blasNodeBuffer.SetData(data.gpuBvhNodes.data(), 0, data.gpuBvhNodes.size());
               
                bvhTriangleBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(GPUBVHTriangle));
                bvhTriangleBuffer.SetSize(data.gpuBvhTriangles.size());
                bvhTriangleBuffer.SetData(data.gpuBvhTriangles.data(), 0, data.gpuBvhTriangles.size());
            }
            else {
                Graphics::ASBuilder asBuilder;

                std::vector<Graphics::ASGeometryRegion> geometryRegions;
                for (auto& subData : data.subData) {
                    geometryRegions.emplace_back(Graphics::ASGeometryRegion{
                        .indexCount = subData.indicesCount,
                        .indexOffset = subData.indicesOffset,
                        .opaque = !subData.material->HasOpacityMap() && subData.material->opacity == 1.0f
                        });
                }

                auto blasDesc = asBuilder.GetBLASDescForTriangleGeometry(vertexBuffer.buffer, indexBuffer.buffer,
                    vertexBuffer.elementCount, vertexBuffer.elementSize, indexBuffer.elementSize, geometryRegions);

                blas = device->CreateBLAS(blasDesc);

                std::vector<uint32_t> triangleOffsets;
                for (auto& subData : data.subData) {
                    auto triangleOffset = subData.indicesOffset / 3;
                    triangleOffsets.push_back(triangleOffset);
                }

                triangleOffsetBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(uint32_t));
                triangleOffsetBuffer.SetSize(triangleOffsets.size(), triangleOffsets.data());

                needsBvhRefresh = true;

            }

            isBvhBuilt = true;

        }

        bool Mesh::IsBVHBuilt() const {

            return isBvhBuilt;

        }

    }

}