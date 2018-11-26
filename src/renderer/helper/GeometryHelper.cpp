#include "GeometryHelper.h"

VertexArray* GeometryHelper::GenerateRectangleVertexArray() {

	VertexArray* vertexArray = new VertexArray();
	VertexBuffer* buffer = new VertexBuffer(GL_ARRAY_BUFFER, GL_BYTE, 2);
	buffer->SetData(&rectangleVertices[0], 8);
	vertexArray->AddComponent(0, buffer);

	return vertexArray;

}

VertexArray* GeometryHelper::GenerateCubeVertexArray() {

	VertexArray* vertexArray = new VertexArray();
	VertexBuffer* vertexBuffer = new VertexBuffer(GL_ARRAY_BUFFER, GL_FLOAT, 3);
	vertexBuffer->SetData(&cubeVertices[0], 108);
	vertexArray->AddComponent(0, vertexBuffer);

	return vertexArray;

}

VertexArray* GeometryHelper::GenerateSphereVertexArray(uint32_t rings, uint32_t segments) {

	uint32_t* indices = nullptr;
	vec3* vertices = nullptr;

	uint32_t indexCount, vertexCount;

	// The sphere is generated with triangles that are in clockwise order
	// This helps us for both atmospheric and point light rendering
	GenerateSphere(rings, segments, indices, vertices, &indexCount, &vertexCount);

	VertexArray* vertexArray = new VertexArray();
	VertexBuffer* indicesBuffer = new VertexBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_UNSIGNED_INT, 1);
	VertexBuffer* verticesBuffer = new VertexBuffer(GL_ARRAY_BUFFER, GL_FLOAT, 3);
	indicesBuffer->SetData(indices, indexCount);
	verticesBuffer->SetData(vertices, vertexCount);
	vertexArray->AddIndexComponent(indicesBuffer);
	vertexArray->AddComponent(0, verticesBuffer);

	return vertexArray;

}

void GeometryHelper::GenerateSphere(uint32_t rings, uint32_t segments, uint32_t*& indicies, vec3*& vertices,
	uint32_t* indexCount, uint32_t* vertexCount) {

	rings = rings < 3 ? 3 : rings;
	segments = segments < 3 ? 3 : segments;

	uint32_t innerRings = rings - 2;
	*vertexCount = innerRings * segments + 2;
	*indexCount = (innerRings - 1) * segments * 6 + 2 * segments * 3;

	vertices = new vec3[*vertexCount];
	indicies = new uint32_t[*indexCount];

	// Set the two outer rings
	vertices[0] = vec3(0.0f, 1.0f, 0.0f);
	vertices[*vertexCount - 1] = vec3(0.0f, -1.0f, 0.0f);

	const float pi = 3.14159265359f;
	float fRings = (float)rings;
	float fSegments = (float)segments;

	uint32_t vertexIndex = 1;

	float alpha = pi / (fRings - 1);

	// Generate the vertices for the inner rings
	for (uint32_t i = 0; i < innerRings; i++) {

		float ringRadius = sinf(alpha);
		float ringHeight = cosf(alpha);
		float beta = 0.0f;

		for (uint32_t j = 0; j < segments; j++) {

			float x = sinf(beta) * ringRadius;
			float z = cosf(beta) * ringRadius;
			vertices[vertexIndex++] = vec3(x, ringHeight, z);

			beta += 2.0f * pi / fSegments;

		}

		alpha += pi / (fRings - 1);

	}

	uint32_t indexIndex = 0;

	// Indices for the upper outer ring
	for (uint32_t i = 0; i < segments; i++) {
		indicies[indexIndex++] = i + 1;
		indicies[indexIndex++] = 0;
		indicies[indexIndex++] = (i + 1) % segments + 1;
	}
	
	// Indices for the lower outer ring
	for (uint32_t i = 0; i < segments; i++) {
		indicies[indexIndex++] = *vertexCount - 1 - segments + (i + 1) % segments;
		indicies[indexIndex++] = *vertexCount - 1;
		indicies[indexIndex++] = *vertexCount - 1 - segments + i;
	}
	
	uint32_t offset = 1;
	
	// Generate the indices for the inner rings
	for (uint32_t i = 0; i < innerRings - 1; i++) {
		offset += segments;
		for (uint32_t j = 0; j < segments; j++) {
			indicies[indexIndex++] = offset - segments + j;
			indicies[indexIndex++] = offset + (j + 1) % segments;
			indicies[indexIndex++] = offset + j;
			indicies[indexIndex++] = offset - segments + (j + 1) % segments;
			indicies[indexIndex++] = offset + (j + 1) % segments;
			indicies[indexIndex++] = offset - segments + j;
		}
	}

}

int8_t GeometryHelper::rectangleVertices[] = {
	-1, -1,
	1, -1,
	-1, 1,
	1, 1
};

float GeometryHelper::cubeVertices[] = {
	-1.0f,  1.0f, -1.0f,
	-1.0f, -1.0f, -1.0f,
	1.0f, -1.0f, -1.0f,
	1.0f, -1.0f, -1.0f,
	1.0f,  1.0f, -1.0f,
	-1.0f,  1.0f, -1.0f,

	-1.0f, -1.0f,  1.0f,
	-1.0f, -1.0f, -1.0f,
	-1.0f,  1.0f, -1.0f,
	-1.0f,  1.0f, -1.0f,
	-1.0f,  1.0f,  1.0f,
	-1.0f, -1.0f,  1.0f,

	1.0f, -1.0f, -1.0f,
	1.0f, -1.0f,  1.0f,
	1.0f,  1.0f,  1.0f,
	1.0f,  1.0f,  1.0f,
	1.0f,  1.0f, -1.0f,
	1.0f, -1.0f, -1.0f,

	-1.0f, -1.0f,  1.0f,
	-1.0f,  1.0f,  1.0f,
	1.0f,  1.0f,  1.0f,
	1.0f,  1.0f,  1.0f,
	1.0f, -1.0f,  1.0f,
	-1.0f, -1.0f,  1.0f,

	-1.0f,  1.0f, -1.0f,
	1.0f,  1.0f, -1.0f,
	1.0f,  1.0f,  1.0f,
	1.0f,  1.0f,  1.0f,
	-1.0f,  1.0f,  1.0f,
	-1.0f,  1.0f, -1.0f,

	-1.0f, -1.0f, -1.0f,
	-1.0f, -1.0f,  1.0f,
	1.0f, -1.0f, -1.0f,
	1.0f, -1.0f, -1.0f,
	-1.0f, -1.0f,  1.0f,
	1.0f, -1.0f,  1.0f
};