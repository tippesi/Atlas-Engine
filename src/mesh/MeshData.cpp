#include "MeshData.h"

namespace Atlas {

	namespace Mesh {

		MeshData::MeshData(const MeshData& that) {

			DeepCopy(that);

		}

		MeshData::MeshData() {

			indices = DataComponent<uint32_t, void>(AE_COMPONENT_UNSIGNED_INT, 1);

			vertices = DataComponent<float, float>(AE_COMPONENT_FLOAT, 3);
			texCoords = DataComponent<float, float16>(AE_COMPONENT_FLOAT, 2);
			normals = DataComponent<float, uint32_t>(AE_COMPONENT_FLOAT, 4);
			tangents = DataComponent<float, uint32_t>(AE_COMPONENT_FLOAT, 4);

		}

		MeshData& MeshData::operator=(const MeshData& that) {

			if (this != &that) {

				DeepCopy(that);

			}

			return *this;

		}

		void MeshData::SetIndexCount(int32_t count) {

			indices.SetSize(count);

			indexCount = count;

		}

		int32_t MeshData::GetIndexCount() const {

			return indexCount;

		}

		void MeshData::SetVertexCount(int32_t count) {

			vertices.SetSize(count);
			texCoords.SetSize(count);
			normals.SetSize(count);
			tangents.SetSize(count);

			vertexCount = count;

		}

		int32_t MeshData::GetVertexCount() const {

			return vertexCount;

		}

		void MeshData::SetTransform(mat4 transform) {

			auto hasNormals = normals.ContainsData();
			auto hasTangents = tangents.ContainsData();

			auto vertex = vertices.Get();
			auto normal = normals.Get();
			auto tangent = tangents.Get();

			auto matrix = transform /** glm::inverse(this->transform)*/;

			auto min = vec3(std::numeric_limits<float>::max());
			auto max = vec3(-std::numeric_limits<float>::max());

			for (int32_t i = 0; i < vertexCount; i++) {

				auto v = vec4(vertex[i * 3], vertex[i * 3 + 1],
					vertex[i * 3 + 2], 1.0f);

				v = matrix * v;

				min = glm::min(min, vec3(v));
				max = glm::max(max, vec3(v));

				vertex[i * 3] = v.x;
				vertex[i * 3 + 1] = v.y;
				vertex[i * 3 + 2] = v.z;

				if (hasNormals) {
					auto n = vec4(normal[i * 3], normal[i * 3 + 1],
						normal[i * 3 + 2], 0.0f);

					n = vec4(normalize(vec3(matrix * n)), 0.0f);

					normal[i * 3] = n.x;
					normal[i * 3 + 1] = n.y;
					normal[i * 3 + 2] = n.z;
				}
				if (hasTangents) {
					auto t = vec4(tangent[i * 3], tangent[i * 3 + 1],
						tangent[i * 3 + 2], 0.0f);

					t = vec4(normalize(vec3(matrix * t)), 0.0f);

					tangent[i * 3] = t.x;
					tangent[i * 3 + 1] = t.y;
					tangent[i * 3 + 2] = t.z;
				}

			}

			vertices.Set(vertex);
			normals.Set(normal);
			tangents.Set(tangent);

			aabb = Volume::AABB(min, max);

			this->transform = transform;

		}

		void MeshData::DeepCopy(const MeshData& that) {

			indices = that.indices;

			vertices = that.vertices;
			texCoords = that.texCoords;
			normals = that.normals;
			tangents = that.tangents;

			indexCount = that.indexCount;
			vertexCount = that.vertexCount;

			primitiveType = that.primitiveType;

			aabb = that.aabb;

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