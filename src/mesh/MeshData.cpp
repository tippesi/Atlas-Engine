#include "MeshData.h"

MeshData::MeshData() {

	indices = new DataComponent<uint32_t, void>(COMPONENT_UNSIGNED_INT, 1);

	vertices = new DataComponent<float, float>(COMPONENT_FLOAT, 3);
	texCoords = new DataComponent<float, float16_t>(COMPONENT_FLOAT, 2);
	normals = new DataComponent<float, uint32_t>(COMPONENT_FLOAT, 3);
	tangents = new DataComponent<float, uint32_t>(COMPONENT_FLOAT, 3);

	indexCount = 0;
	vertexCount = 0;

	primitiveType = PRIMITIVE_TRIANGLES;

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

MeshData::~MeshData() {

	delete indices;
	delete vertices;
	delete texCoords;
	delete normals;
	delete tangents;

}