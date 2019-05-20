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