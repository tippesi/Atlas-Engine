#include "MeshData.h"

#include "../volume/BVH.h"

namespace Atlas {

    namespace Mesh {

        MeshData::MeshData(const MeshData& that) {

            DeepCopy(that);

        }

        MeshData::MeshData(MeshDataUsage usage) : usage(usage) {

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

            if (hasTangents)
                tangents.Set(tangent);

            aabb = Volume::AABB(min, max);

            this->transform = transform;

        }

        void MeshData::UpdateData() {

            bool hostAccessible = usage & MeshDataUsageBits::HostAccessBit;

            if (indices.ContainsData()) {
                auto type = indices.GetElementSize() == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
                indexBuffer = Buffer::IndexBuffer(type, GetIndexCount(),
                    indices.GetConvertedVoid(), hostAccessible);
            }
            if (vertices.ContainsData()) {
                vertexBuffer = Buffer::VertexBuffer(vertices.GetFormat(), GetVertexCount(),
                    vertices.GetConvertedVoid(), hostAccessible);
            }
            if (normals.ContainsData()) {
                normalBuffer = Buffer::VertexBuffer(normals.GetFormat(), GetVertexCount(),
                    normals.GetConvertedVoid(), hostAccessible);
            }
            if (texCoords.ContainsData()) {
                texCoordBuffer = Buffer::VertexBuffer(texCoords.GetFormat(), GetVertexCount(),
                    texCoords.GetConvertedVoid(), hostAccessible);
            }
            if (tangents.ContainsData()) {
                tangentBuffer = Buffer::VertexBuffer(tangents.GetFormat(), GetVertexCount(),
                    tangents.GetConvertedVoid(), hostAccessible);
            }
            if (colors.ContainsData()) {
                colorBuffer = Buffer::VertexBuffer(colors.GetFormat(), GetVertexCount(),
                    colors.GetConvertedVoid(), hostAccessible);
            }

            UpdateVertexArray();

        }

        void MeshData::UpdateVertexArray() {

            vertexArray.AddIndexComponent(indexBuffer);
            vertexArray.AddComponent(0, vertexBuffer);
            if (normals.ContainsData())
                vertexArray.AddComponent(1, normalBuffer);
            if (texCoords.ContainsData())
                vertexArray.AddComponent(2, texCoordBuffer);
            if (tangents.ContainsData())
                vertexArray.AddComponent(3, tangentBuffer);
            if (colors.ContainsData())
                vertexArray.AddComponent(4, colorBuffer);

        }

        void MeshData::DeepCopy(const MeshData& that) {

            filename = that.filename;

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