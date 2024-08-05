#include "Mesh.h"
#include "../common/Path.h"

#include "../renderer/OpaqueRenderer.h"
#include "../renderer/ShadowRenderer.h"

#include "../graphics/ASBuilder.h"
#include "../volume/BVH.h"

namespace Atlas {

    namespace Mesh {

        Mesh::Mesh(const ResourceHandle<MeshData>& meshData, std::vector<uint32_t> subDataIndices, MeshMobility mobility)
            : data(meshData), subDataIndices(subDataIndices), mobility(mobility) {

            Update();

        }

        Mesh::Mesh(MeshMobility mobility) : mobility(mobility) {

            

        }

        void Mesh::Update() {

            if (!data.IsLoaded())
                return;

            if (subDataIndices.empty()) {
                subDataIndices.resize(data->subData.size());
                std::iota(std::begin(subDataIndices), std::end(subDataIndices), 0);
            }

            aabb = data->subData[subDataIndices.front()].aabb;
            for (size_t i = 0; i < subDataIndices.size(); i++) {
                aabb.Grow(data->subData[subDataIndices[i]].aabb);
            }

            radius = glm::length(aabb.max - aabb.min) * 0.5;

        }

        void Mesh::UpdateMaterials() {

            for (const auto& material : data->materials) {
                material->SetChanged();
            }

        }

        void Mesh::BuildBVH(bool parallelBuild) {

            if (!data.IsLoaded())
                return;

            auto device = Graphics::GraphicsDevice::DefaultDevice;
            bool hardwareRayTracing = device->support.hardwareRayTracing;
            bool bindless = device->support.bindless;

            AE_ASSERT(data->indexCount > 0 && "There is no data in this mesh");

            if (data->indexCount == 0 || !bindless) return;

            BuildBVHData(parallelBuild);

            triangleBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit | Buffer::BufferUsageBits::DedicatedMemoryBit, sizeof(GPUTriangle));
            triangleBuffer.SetSize(gpuTriangles.size());
            triangleBuffer.SetData(gpuTriangles.data(), 0, gpuTriangles.size());

            if (!hardwareRayTracing) {
                blasNodeBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit | Buffer::BufferUsageBits::DedicatedMemoryBit, sizeof(GPUBVHNode));
                blasNodeBuffer.SetSize(gpuBvhNodes.size());
                blasNodeBuffer.SetData(gpuBvhNodes.data(), 0, gpuBvhNodes.size());
               
                bvhTriangleBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit | Buffer::BufferUsageBits::DedicatedMemoryBit, sizeof(GPUBVHTriangle));
                bvhTriangleBuffer.SetSize(gpuBvhTriangles.size());
                bvhTriangleBuffer.SetData(gpuBvhTriangles.data(), 0, gpuBvhTriangles.size());
            }
            else {
                Graphics::ASBuilder asBuilder;

                std::vector<Graphics::ASGeometryRegion> geometryRegions;
                for (auto subDataIdx : subDataIndices) {
                    auto& subData = data->subData[subDataIdx];
                    geometryRegions.emplace_back(Graphics::ASGeometryRegion{
                        .indexCount = subData.indicesCount,
                        .indexOffset = subData.indicesOffset,
                        .opaque = !subData.material->HasOpacityMap() && subData.material->opacity == 1.0f
                        });
                }

                auto blasDesc = asBuilder.GetBLASDescForTriangleGeometry(data->vertexBuffer.buffer, data->indexBuffer.buffer,
                    data->vertexBuffer.elementCount, data->vertexBuffer.elementSize, data->indexBuffer.elementSize, geometryRegions);

                blas = device->CreateBLAS(blasDesc);

                std::vector<uint32_t> triangleOffsets;
                for (const auto& subData : data->subData) {
                    auto triangleOffset = subData.indicesOffset / 3;
                    triangleOffsets.push_back(triangleOffset);
                }

                triangleOffsetBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit | Buffer::BufferUsageBits::DedicatedMemoryBit, sizeof(uint32_t));
                triangleOffsetBuffer.SetSize(triangleOffsets.size(), triangleOffsets.data());

                needsBvhRefresh = true;
            }

            isBvhBuilt = true;

            // We can't clear the gpu triangles, they are used elsewhere
            gpuBvhNodes.clear();
            gpuBvhNodes.shrink_to_fit();

            gpuBvhTriangles.clear();
            gpuBvhTriangles.shrink_to_fit();

        }

        bool Mesh::IsBVHBuilt() const {

            return isBvhBuilt;

        }

        void Mesh::BuildBVHData(bool parallelBuild) {

            auto device = Graphics::GraphicsDevice::DefaultDevice;
            bool hardwareRayTracing = device->support.hardwareRayTracing;

            struct Triangle {
                vec3 v0;
                vec3 v1;
                vec3 v2;

                vec3 n0;
                vec3 n1;
                vec3 n2;

                vec2 uv0;
                vec2 uv1;
                vec2 uv2;

                vec4 color0;
                vec4 color1;
                vec4 color2;

                int32_t materialIdx;
                float opacity;
            };

            uint32_t triangleCount = 0;

            for (auto subDataIdx : subDataIndices) {
                auto& sub = data->subData[subDataIdx];
                triangleCount += sub.indicesCount / 3;
            }

            std::vector<Triangle> triangles(triangleCount);

            std::vector<Volume::AABB> aabbs(triangleCount);
            std::vector<Volume::BVHTriangle> bvhTriangles(triangleCount);

            triangleCount = 0;

            for (auto subDataIdx : subDataIndices) {

                auto& sub = data->subData[subDataIdx];
                auto subDataTriangleCount = sub.indicesCount / 3;

                for (uint32_t i = 0; i < subDataTriangleCount; i++) {

                    auto k = i + triangleCount;

                    auto idx0 = data->indices.Get()[k * 3];
                    auto idx1 = data->indices.Get()[k * 3 + 1];
                    auto idx2 = data->indices.Get()[k * 3 + 2];

                    // Transform everything
                    triangles[k].v0 = data->vertices.Get()[idx0];
                    triangles[k].v1 = data->vertices.Get()[idx1];
                    triangles[k].v2 = data->vertices.Get()[idx2];

                    triangles[k].n0 = normalize(data->normals.Get()[idx0]);
                    triangles[k].n1 = normalize(data->normals.Get()[idx1]);
                    triangles[k].n2 = normalize(data->normals.Get()[idx2]);

                    if (data->texCoords.ContainsData()) {
                        triangles[k].uv0 = data->texCoords.Get()[idx0];
                        triangles[k].uv1 = data->texCoords.Get()[idx1];
                        triangles[k].uv2 = data->texCoords.Get()[idx2];
                    }

                    if (data->colors.ContainsData()) {
                        triangles[k].color0 = data->colors.Get()[idx0];
                        triangles[k].color1 = data->colors.Get()[idx1];
                        triangles[k].color2 = data->colors.Get()[idx2];
                    }
                    else {
                        triangles[k].color0 = vec4(1.0f);
                        triangles[k].color1 = vec4(1.0f);
                        triangles[k].color2 = vec4(1.0f);
                    }

                    triangles[k].materialIdx = sub.materialIdx;
                    triangles[k].opacity = sub.material->HasOpacityMap() ? -1.0f : sub.material->opacity;

                    auto min = glm::min(glm::min(triangles[k].v0,
                        triangles[k].v1), triangles[k].v2);
                    auto max = glm::max(glm::max(triangles[k].v0,
                        triangles[k].v1), triangles[k].v2);

                    bvhTriangles[k].v0 = triangles[k].v0;
                    bvhTriangles[k].v1 = triangles[k].v1;
                    bvhTriangles[k].v2 = triangles[k].v2;
                    bvhTriangles[k].idx = k;

                    aabbs[k] = Volume::AABB(min, max);

                }

                triangleCount += subDataTriangleCount;

            }

            Volume::BVH bvh;
            if (!hardwareRayTracing) {
                // Generate BVH
                bvh = Volume::BVH(aabbs, bvhTriangles, parallelBuild);

                bvhTriangles.clear();
                bvhTriangles.shrink_to_fit();
            }

            auto& data = hardwareRayTracing ? bvhTriangles : bvh.data;

            for (auto& bvhTriangle : data) {

                auto& triangle = triangles[bvhTriangle.idx];

                auto v0v1 = triangle.v1 - triangle.v0;
                auto v0v2 = triangle.v2 - triangle.v0;

                auto uv0uv1 = triangle.uv1 - triangle.uv0;
                auto uv0uv2 = triangle.uv2 - triangle.uv0;

                auto r = 1.0f / (uv0uv1.x * uv0uv2.y - uv0uv2.x * uv0uv1.y);

                auto s = vec3(uv0uv2.y * v0v1.x - uv0uv1.y * v0v2.x,
                    uv0uv2.y * v0v1.y - uv0uv1.y * v0v2.y,
                    uv0uv2.y * v0v1.z - uv0uv1.y * v0v2.z) * r;

                auto t = vec3(uv0uv1.x * v0v2.x - uv0uv2.x * v0v1.x,
                    uv0uv1.x * v0v2.y - uv0uv2.x * v0v1.y,
                    uv0uv1.x * v0v2.z - uv0uv2.x * v0v1.z) * r;

                auto normal = glm::normalize(triangle.n0 + triangle.n1 + triangle.n2);

                auto tangent = glm::normalize(s - normal * dot(normal, s));
                auto handedness = (glm::dot(glm::cross(tangent, normal), t) < 0.0f ? 1.0f : -1.0f);

                auto bitangent = handedness * glm::normalize(glm::cross(tangent, normal));

                // Compress data
                auto pn0 = Common::Packing::PackSignedVector3x10_1x2(vec4(triangle.n0, 0.0f));
                auto pn1 = Common::Packing::PackSignedVector3x10_1x2(vec4(triangle.n1, 0.0f));
                auto pn2 = Common::Packing::PackSignedVector3x10_1x2(vec4(triangle.n2, 0.0f));

                auto pt = Common::Packing::PackSignedVector3x10_1x2(vec4(tangent, 0.0f));
                auto pbt = Common::Packing::PackSignedVector3x10_1x2(vec4(bitangent, 0.0f));

                auto puv0 = glm::packHalf2x16(triangle.uv0);
                auto puv1 = glm::packHalf2x16(triangle.uv1);
                auto puv2 = glm::packHalf2x16(triangle.uv2);

                auto pc0 = glm::packUnorm4x8(triangle.color0);
                auto pc1 = glm::packUnorm4x8(triangle.color1);
                auto pc2 = glm::packUnorm4x8(triangle.color2);

                auto cn0 = reinterpret_cast<float&>(pn0);
                auto cn1 = reinterpret_cast<float&>(pn1);
                auto cn2 = reinterpret_cast<float&>(pn2);

                auto ct = reinterpret_cast<float&>(pt);
                auto cbt = reinterpret_cast<float&>(pbt);

                auto cuv0 = reinterpret_cast<float&>(puv0);
                auto cuv1 = reinterpret_cast<float&>(puv1);
                auto cuv2 = reinterpret_cast<float&>(puv2);

                auto cc0 = reinterpret_cast<float&>(pc0);
                auto cc1 = reinterpret_cast<float&>(pc1);
                auto cc2 = reinterpret_cast<float&>(pc2);

                GPUTriangle gpuTriangle;

                gpuTriangle.v0 = vec4(triangle.v0, cn0);
                gpuTriangle.v1 = vec4(triangle.v1, cn1);
                gpuTriangle.v2 = vec4(triangle.v2, cn2);
                gpuTriangle.d0 = vec4(cuv0, cuv1, cuv2, reinterpret_cast<float&>(triangle.materialIdx));
                gpuTriangle.d1 = vec4(ct, cbt, bvhTriangle.endOfNode ? 1.0f : -1.0f, 0.0f);
                gpuTriangle.d2 = vec4(cc0, cc1, cc2, triangle.opacity);

                gpuTriangles.push_back(gpuTriangle);

                if (!hardwareRayTracing) {
                    GPUBVHTriangle gpuBvhTriangle;
                    gpuBvhTriangle.v0 = vec4(triangle.v0, bvhTriangle.endOfNode ? 1.0f : -1.0f);
                    gpuBvhTriangle.v1 = vec4(triangle.v1, reinterpret_cast<float&>(triangle.materialIdx));
                    gpuBvhTriangle.v2 = vec4(triangle.v2, triangle.opacity);

                    gpuBvhTriangles.push_back(gpuBvhTriangle);
                }

            }

            if (!hardwareRayTracing) {
                triangles.clear();
                triangles.shrink_to_fit();

                auto& nodes = bvh.GetTree();
                gpuBvhNodes = std::vector<GPUBVHNode>(nodes.size());
                // Copy to GPU format
                for (size_t i = 0; i < nodes.size(); i++) {
                    gpuBvhNodes[i].leftPtr = nodes[i].leftPtr;
                    gpuBvhNodes[i].rightPtr = nodes[i].rightPtr;

                    gpuBvhNodes[i].leftAABB.min = nodes[i].leftAABB.min;
                    gpuBvhNodes[i].leftAABB.max = nodes[i].leftAABB.max;

                    gpuBvhNodes[i].rightAABB.min = nodes[i].rightAABB.min;
                    gpuBvhNodes[i].rightAABB.max = nodes[i].rightAABB.max;
                }
            }

        }

    }

}