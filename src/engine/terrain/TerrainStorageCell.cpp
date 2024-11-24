#include "TerrainStorageCell.h"
#include "TerrainStorage.h"

namespace Atlas {

    namespace Terrain {

        TerrainStorageCell::TerrainStorageCell(TerrainStorage* storage) : storage(storage) {

            

        }

        TerrainStorageCell::TerrainStorageCell(const TerrainStorageCell& that) {

            if (this != &that) {

                // Due to the atomic bool we need to do the copying manually (this method should ONLY be called when initializing the terrain)
                this->x = that.x;
                this->y = that.y;
                this->LoD = that.LoD;

                this->position = that.position;
                this->inverseMatrix = that.inverseMatrix;

                this->heightData = that.heightData;
                this->materialIdxData = that.materialIdxData;
                this->normalData = that.normalData;

                this->heightField = that.heightField;
                this->normalMap = that.normalMap;
                this->splatMap = that.splatMap;

                this->storage = that.storage;

                this->isLoaded.store(that.isLoaded.load());

            }

        }

        bool TerrainStorageCell::IsLoaded() {

            return isLoaded;

        }

        void TerrainStorageCell::Unload() {

            // We keep the height data, even after unload (e.g. needed for ray casting, height estimation)
            materialIdxData.clear();
            normalData.clear();

            materialIdxData.shrink_to_fit();
            normalData.shrink_to_fit();

            heightField = nullptr;
            normalMap = nullptr;
            splatMap = nullptr;

            blas.reset();

            isLoaded = false;

        }

        void TerrainStorageCell::BuildBVH(float stretchFactor, float heightFactor) {

            if (!IsLoaded()) return;

            int32_t heightFieldSideLength = int32_t(sqrtf(float(heightData.size())));

            aabb.min = glm::vec3(std::numeric_limits<float>::max());
            aabb.max = glm::vec3(-std::numeric_limits<float>::max());

            std::vector<vec3> vertices(heightData.size());
            for (int32_t y = 0; y < heightFieldSideLength; y++) {
                for (int32_t x = 0; x < heightFieldSideLength; x++) {
                    auto idx = y * heightFieldSideLength + x;
                    vertices[idx] = vec3(float(x) * stretchFactor, heightData[idx] * heightFactor, float(y) * stretchFactor);

                    aabb.min = glm::min(aabb.min, vertices[idx]);
                    aabb.max = glm::max(aabb.max, vertices[idx]);
                }
            }

            auto vertexSideCount = heightFieldSideLength - 1;
            AE_ASSERT(vertexSideCount > 0);

            std::vector<uint32_t> indices(vertexSideCount * vertexSideCount * 6);
            for (int32_t y = 0; y < vertexSideCount; y++) {
                for (int32_t x = 0; x < vertexSideCount; x++) {
                    auto idx = y * heightFieldSideLength + x;
                    auto baseIdx = (y * vertexSideCount + x) * 6;

                    indices[baseIdx + 0] = idx;
                    indices[baseIdx + 1] = idx + 1;
                    indices[baseIdx + 2] = idx + heightFieldSideLength;

                    indices[baseIdx + 3] = idx + 1;
                    indices[baseIdx + 4] = idx + heightFieldSideLength + 1;
                    indices[baseIdx + 5] = idx + heightFieldSideLength;
                }
            }

            Buffer::IndexBuffer indexBuffer(VK_INDEX_TYPE_UINT32, indices.size(), indices.data(), true);
            Buffer::VertexBuffer vertexBuffer(VK_FORMAT_R32G32B32_SFLOAT, vertices.size(), vertices.data(), true);

            std::vector<ResourceHandle<Material>> materials;
            for (size_t i = 0; i < storage->materials.size(); i++) {
                if (storage->materials[i].IsLoaded()) {
                    materials.push_back(storage->materials[i]);
                    materials.back()->twoSided = false;
                }
                else {
                    materials.push_back(ResourceHandle<Material>());
                }
            }

            std::vector<RayTracing::BLAS::Triangle> triangles(indices.size() / 3);
            for (size_t i = 0; i < triangles.size(); i++) {
                
                RayTracing::BLAS::Triangle triangle;

                triangle.v0 = vertices[indices[i * 3 + 0]];
                triangle.v1 = vertices[indices[i * 3 + 1]];
                triangle.v2 = vertices[indices[i * 3 + 2]];

                vec3 normal = -glm::normalize(glm::cross(triangle.v0 - triangle.v1, triangle.v0 - triangle.v2));

                normal *= normal.y < 0.0f ? -1.0f : 1.0f;

                triangle.n0 = normal;
                triangle.n1 = normal;
                triangle.n2 = normal;

                triangle.uv0 = vec2(triangle.v0.x, triangle.v0.z);
                triangle.uv1 = vec2(triangle.v1.x, triangle.v1.z);
                triangle.uv2 = vec2(triangle.v2.x, triangle.v2.z);

                triangle.materialIdx = int32_t(materialIdxData[indices[i * 3]]);

                triangles[i] = triangle;
            }

            Graphics::ASGeometryRegion geometryRegions[] = {{
                .indexCount = indices.size(),
                .indexOffset = 0,
                .opaque = true
            }};

            auto blas = CreateRef<RayTracing::BLAS>();
            blas->Build(triangles, materials, vertexBuffer, indexBuffer,  geometryRegions);

            if (Graphics::GraphicsDevice::DefaultDevice->support.hardwareRayTracing) {
                Graphics::ASBuilder asBuilder;
                std::vector<Ref<Graphics::BLAS>> blases = { blas->blas };
                asBuilder.BuildBLAS(blases);

                blas->blas = blases.front();
                blas->needsBvhRefresh = false;
            }

            this->blas = blas;

        }

    }

}