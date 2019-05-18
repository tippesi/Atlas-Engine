#include "MeshData.h"

namespace Atlas {

	namespace Mesh {

		MeshData::MeshData() {

			indices = DataComponent<uint32_t, void>(AE_COMPONENT_UNSIGNED_INT, 1);

			vertices = DataComponent<float, float>(AE_COMPONENT_FLOAT, 3);
			texCoords = DataComponent<float, float16>(AE_COMPONENT_FLOAT, 2);
			normals = DataComponent<float, uint32_t>(AE_COMPONENT_FLOAT, 3);
			tangents = DataComponent<float, uint32_t>(AE_COMPONENT_FLOAT, 3);

			indexCount = 0;
			vertexCount = 0;

			primitiveType = AE_PRIMITIVE_TRIANGLES;

		}

		MeshData::~MeshData() {

			

		}

		void MeshData::SetIndexCount(int32_t count) {

			indices.SetSize(count);

			indexCount = count;

		}

		int32_t MeshData::GetIndexCount() {

			return indexCount;

		}

		void MeshData::SetVertexCount(int32_t count) {

			vertices.SetSize(count);
			texCoords.SetSize(count);
			normals.SetSize(count);
			tangents.SetSize(count);

			vertexCount = count;

		}

		int32_t MeshData::GetVertexCount() {

			return vertexCount;

		}

	}

}