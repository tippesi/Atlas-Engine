#include "MeshData.h"

#include "../volume/BVH.h"

namespace Atlas {

    namespace Mesh {

        MeshData::MeshData(const MeshData& that) {

            DeepCopy(that);

        }

        MeshData::MeshData() {

            indices = DataComponent<uint32_t>(ComponentFormat::UnsignedInt);

            vertices = DataComponent<vec3>(ComponentFormat::Float);
            texCoords = DataComponent<vec2>(ComponentFormat::HalfFloat);
            normals = DataComponent<vec4>(ComponentFormat::PackedFloat);
            tangents = DataComponent<vec4>(ComponentFormat::PackedFloat);

        }

        MeshData& MeshData::operator=(const MeshData& that) {

            if (this != &that) {

                DeepCopy(that);

            }

            return *this;

        }

        void MeshData::SetIndexCount(int32_t count) {

            indexCount = count;

        }

        int32_t MeshData::GetIndexCount() const {

            return indexCount;

        }

        void MeshData::SetVertexCount(int32_t count) {

            vertexCount = count;

        }

        int32_t MeshData::GetVertexCount() const {

            return vertexCount;

        }

        void MeshData::SetTransform(mat4 transform) {

            auto hasNormals = normals.ContainsData();
            auto hasTangents = tangents.ContainsData();

            auto& vertex = vertices.Get();
            auto& normal = normals.Get();
            auto& tangent = tangents.Get();

            auto matrix = transform;

            auto min = vec3(std::numeric_limits<float>::max());
            auto max = vec3(-std::numeric_limits<float>::max());

            for (int32_t i = 0; i < vertexCount; i++) {

                auto v = vec4(vertex[i], 1.0f);

                v = matrix * v;

                min = glm::min(min, vec3(v));
                max = glm::max(max, vec3(v));

                vertex[i] = v;

                if (hasNormals) {
                    auto n = vec4(vec3(normal[i]), 0.0f);

                    n = vec4(normalize(vec3(matrix * n)), normal[i].w);

                    normal[i] = n;
                }
                if (hasTangents) {
                    auto t = vec4(vec3(tangent[i]), 0.0f);

                    t = vec4(normalize(vec3(matrix * t)), tangent[i].w);

                    tangent[i] = t;
                }

            }

            vertices.Set(vertex);

            if (hasNormals)
                normals.Set(normal);

            if(hasTangents)
                tangents.Set(tangent);

            aabb = Volume::AABB(min, max);

            this->transform = transform;

        }

        void MeshData::BuildBVH() {

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

                int32_t materialIdx;
            };

            uint32_t triangleCount = 0;

            for (auto& sub : subData) {
                triangleCount += sub.indicesCount / 3;
            }

            std::vector<Triangle> triangles(triangleCount);

            std::vector<Volume::AABB> aabbs(triangleCount);
            std::vector<Volume::BVHTriangle> bvhTriangles(triangleCount);

            triangleCount = 0;

            for (auto& sub : subData) {

                auto subDataTriangleCount = sub.indicesCount / 3;

                for (uint32_t i = 0; i < subDataTriangleCount; i++) {

                    auto k = i + triangleCount;

                    auto idx0 = indices.Get()[k * 3];
                    auto idx1 = indices.Get()[k * 3 + 1];
                    auto idx2 = indices.Get()[k * 3 + 2];

                    // Transform everything
                    triangles[k].v0 = vertices.Get()[idx0];
                    triangles[k].v1 = vertices.Get()[idx1];
                    triangles[k].v2 = vertices.Get()[idx2];

                    triangles[k].n0 = normalize(normals.Get()[idx0]);
                    triangles[k].n1 = normalize(normals.Get()[idx1]);
                    triangles[k].n2 = normalize(normals.Get()[idx2]);

                    if (texCoords.ContainsData()) {
                        triangles[k].uv0 = texCoords.Get()[idx0];
                        triangles[k].uv1 = texCoords.Get()[idx1];
                        triangles[k].uv2 = texCoords.Get()[idx2];
                    }

                    triangles[k].materialIdx = sub.materialIdx;

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

            // Generate BVH
            auto bvh = Volume::BVH(aabbs, bvhTriangles);

            for (auto& bvhTriangle : bvhTriangles) {

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

                auto cn0 = reinterpret_cast<float&>(pn0);
                auto cn1 = reinterpret_cast<float&>(pn1);
                auto cn2 = reinterpret_cast<float&>(pn2);

                auto ct = reinterpret_cast<float&>(pt);
                auto cbt = reinterpret_cast<float&>(pbt);

                auto cuv0 = reinterpret_cast<float&>(puv0);
                auto cuv1 = reinterpret_cast<float&>(puv1);
                auto cuv2 = reinterpret_cast<float&>(puv2);

                GPUTriangle gpuTriangle;

                gpuTriangle.v0 = vec4(triangle.v0, cn0);
                gpuTriangle.v1 = vec4(triangle.v1, cn1);
                gpuTriangle.v2 = vec4(triangle.v2, cn2);
                gpuTriangle.d0 = vec4(cuv0, cuv1, cuv2, reinterpret_cast<float&>(triangle.materialIdx));
                gpuTriangle.d1 = vec4(ct, cbt, bvhTriangle.endOfNode ? 1.0f : -1.0f, 0.0f);

                gpuTriangles.push_back(gpuTriangle);

                BVHTriangle gpuBvhTriangle;
                gpuBvhTriangle.v0 = vec4(triangle.v0, bvhTriangle.endOfNode ? 1.0f : -1.0f);
                gpuBvhTriangle.v1 = vec4(triangle.v1, reinterpret_cast<float&>(triangle.materialIdx));
                gpuBvhTriangle.v2 = vec4(triangle.v2, 0.0f);

                gpuBvhTriangles.push_back(gpuBvhTriangle);

            }

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

        void MeshData::DeepCopy(const MeshData& that) {

            filename = that.filename;

            indices = that.indices;

            vertices = that.vertices;
            texCoords = that.texCoords;
            normals = that.normals;
            tangents = that.tangents;

            indexCount = that.indexCount;
            vertexCount = that.vertexCount;

            primitiveType = that.primitiveType;

            aabb = that.aabb;

            materials.clear();
            subData.clear();

            materials.resize(that.materials.size());
            subData.resize(that.subData.size());

            // We need to refresh the pointers in the sub data
            for (size_t i = 0; i < that.subData.size(); i++) {
                materials[i] = that.materials[i];
                subData[i] = that.subData[i];
                subData[i].material = &materials[i];
            }

        }

    }

}