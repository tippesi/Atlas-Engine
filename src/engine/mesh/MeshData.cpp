#include "MeshData.h"

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

            vulkanMaterials.clear();
			materials.clear();
			subData.clear();

            vulkanMaterials.resize(that.vulkanMaterials.size());
			materials.resize(that.materials.size());
			subData.resize(that.subData.size());

			// We need to refresh the pointers in the sub data
			for (size_t i = 0; i < that.subData.size(); i++) {
                vulkanMaterials[i] = that.vulkanMaterials[i];
				materials[i] = that.materials[i];
				subData[i] = that.subData[i];
				subData[i].material = &materials[i];
				subData[i].vulkanMaterial = &vulkanMaterials[i];
			}

		}

	}

}