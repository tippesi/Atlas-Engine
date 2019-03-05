#include "MeshData.h"

namespace Atlas {

	namespace Mesh {

		MeshData::MeshData() {

			indices = new DataComponent<uint32_t, void>(AE_COMPONENT_UNSIGNED_INT, 1);

			vertices = new DataComponent<float, float>(AE_COMPONENT_FLOAT, 3);
			texCoords = new DataComponent<float, float16>(AE_COMPONENT_FLOAT, 2);
			normals = new DataComponent<float, uint32_t>(AE_COMPONENT_FLOAT, 3);
			tangents = new DataComponent<float, uint32_t>(AE_COMPONENT_FLOAT, 3);

			indexCount = 0;
			vertexCount = 0;

			primitiveType = AE_PRIMITIVE_TRIANGLES;

		}

		MeshData::~MeshData() {

			delete indices;
			delete vertices;
			delete texCoords;
			delete normals;
			delete tangents;

		}

		void MeshData::SetIndexCount(int32_t count) {

			indices->SetSize(count);

			indexCount = count;

		}

		int32_t MeshData::GetIndexCount() {

			return indexCount;

		}

		void MeshData::SetVertexCount(int32_t count) {

			vertices->SetSize(count);
			texCoords->SetSize(count);
			normals->SetSize(count);
			tangents->SetSize(count);

			vertexCount = count;

		}

		int32_t MeshData::GetVertexCount() {

			return vertexCount;

		}

	}

}