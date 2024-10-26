#include "TerrainStorageCell.h"
#include "TerrainStorage.h"

namespace Atlas {

    namespace Terrain {

        TerrainStorageCell::TerrainStorageCell(TerrainStorage* storage) : storage(storage) {

            

        }

        bool TerrainStorageCell::IsLoaded() {

            if (!heightField.IsValid())
                return false;

            if (!normalMap.IsValid())
                return false;

            return true;

        }

        void TerrainStorageCell::BuildBVH() {

            if (!IsLoaded()) return;

            int32_t heightFieldSideLength = int32_t(sqrtf(float(heightData.size())));

            std::vector<vec3> vertices(heightData.size());
            for (int32_t y = 0; y < heightFieldSideLength; y++) {
                for (int32_t x = 0; x < heightFieldSideLength; x++) {
                    auto idx = y * heightFieldSideLength + x;
                    vertices[idx] = vec3(float(x), heightData[idx], float(y));
                }
            }

            auto vertexSideCount = heightFieldSideLength - 1;

            std::vector<uint32_t> indices(vertexSideCount * vertexSideCount * 6);
            for (int32_t y = 0; y < vertexSideCount; y++) {
                for (int32_t x = 0; x < vertexSideCount; x++) {
                    auto idx = y * vertexSideCount + x;
                    auto baseIdx = (y * vertexSideCount + x) * 6;

                    indices[baseIdx + 0] = idx;
                    indices[baseIdx + 1] = idx + 1;
                    indices[baseIdx + 2] = idx + heightFieldSideLength;

                    indices[baseIdx + 3] = idx + 1;
                    indices[baseIdx + 4] = idx + heightFieldSideLength + 1;
                    indices[baseIdx + 5] = idx + heightFieldSideLength;
                }
            }

            Buffer::IndexBuffer indexBuffer(VK_INDEX_TYPE_UINT32, indices.size(), indices.data());
            Buffer::VertexBuffer vertexBuffer(VK_FORMAT_R32G32B32_SFLOAT, vertices.size(), vertices.data());

            auto material = ResourceHandle<Material>(CreateRef<Material>());

            std::vector<RayTracing::BLAS::Triangle> triangles(indices.size() / 3);
            for (size_t i = 0; i < triangles.size(); i++) {
                
                RayTracing::BLAS::Triangle triangle;

                triangle.v0 = vertices[indices[i * 3 + 0]];
                triangle.v1 = vertices[indices[i * 3 + 1]];
                triangle.v2 = vertices[indices[i * 3 + 2]];

                vec3 normal = glm::normalize(glm::cross(triangle.v0 - triangle.v1, triangle.v0 - triangle.v2));

                triangle.n0 = normal;
                triangle.n1 = normal;
                triangle.n2 = normal;

                triangle.materialIdx = 0;

                triangles[i] = triangle;
            }

            Graphics::ASGeometryRegion geometryRegions[] = {{
                .indexCount = indices.size(),
                .indexOffset = 0,
                .opaque = false
            }};

            std::vector<ResourceHandle<Material>> materials = { material };

            blas = CreateRef<RayTracing::BLAS>();
            blas->Build(triangles, materials, vertexBuffer, indexBuffer,  geometryRegions);

        }

    }

}