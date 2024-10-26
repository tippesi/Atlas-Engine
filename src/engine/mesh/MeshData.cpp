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
            normals = DataComponent<vec4>(ComponentFormat::PackedNormal);
            tangents = DataComponent<vec4>(ComponentFormat::PackedNormal);
            colors = DataComponent<vec4>(ComponentFormat::PackedColor);

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

        void  MeshData::BuildBVHData(std::vector<RayTracing::BLAS::Triangle>& triangles) {

            auto device = Graphics::GraphicsDevice::DefaultDevice;
            bool hardwareRayTracing = device->support.hardwareRayTracing;

            uint32_t triangleCount = 0;

            for (auto& sub : subData) {
                triangleCount += sub.indicesCount / 3;
            }

            triangles.resize(triangleCount);

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

                    if (colors.ContainsData()) {
                        triangles[k].color0 = colors.Get()[idx0];
                        triangles[k].color1 = colors.Get()[idx1];
                        triangles[k].color2 = colors.Get()[idx2];
                    }
                    else {
                        triangles[k].color0 = vec4(1.0f);
                        triangles[k].color1 = vec4(1.0f);
                        triangles[k].color2 = vec4(1.0f);
                    }

                    triangles[k].materialIdx = sub.materialIdx;
                    triangles[k].opacity = sub.material->HasOpacityMap() ? -1.0f : sub.material->opacity;

                }

                triangleCount += subDataTriangleCount;

            }

        }

        void MeshData::DeepCopy(const MeshData& that) {

            name = that.name;

            indices = that.indices;

            vertices = that.vertices;
            texCoords = that.texCoords;
            normals = that.normals;
            tangents = that.tangents;
            colors = that.colors;

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
                subData[i].material = materials[i];
            }

        }

    }

}