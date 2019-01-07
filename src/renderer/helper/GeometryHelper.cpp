#include "GeometryHelper.h"

void GeometryHelper::GenerateRectangleVertexArray(VertexArray& vertexArray) {

	VertexBuffer* buffer = new VertexBuffer(GL_BYTE, 2, sizeof(int8_t) * 2, 8);
	buffer->SetData(&rectangleVertices[0], 0, 8);
	vertexArray.AddComponent(0, buffer);

}

void GeometryHelper::GenerateCubeVertexArray(VertexArray& vertexArray) {

	VertexBuffer* vertexBuffer = new VertexBuffer(GL_FLOAT, 3, sizeof(vec3), 108);
	vertexBuffer->SetData(&cubeVertices[0], 0, 108);
	vertexArray.AddComponent(0, vertexBuffer);

}

void GeometryHelper::GenerateSphereVertexArray(VertexArray& vertexArray, uint32_t rings, uint32_t segments) {

	uint32_t* indices = nullptr;
	vec3* vertices = nullptr;

	uint32_t indexCount, vertexCount;

	// The sphere is generated with triangles that are in clockwise order
	// This helps us for both atmospheric and point light rendering
	GenerateSphere(rings, segments, indices, vertices, &indexCount, &vertexCount);

	IndexBuffer* indicesBuffer = new IndexBuffer(GL_UNSIGNED_INT, sizeof(uint32_t), indexCount);
	VertexBuffer* verticesBuffer = new VertexBuffer(GL_FLOAT, 3, sizeof(vec3), vertexCount);
	indicesBuffer->SetData(&indices[0], 0, indexCount);
	verticesBuffer->SetData(&vertices[0], 0, vertexCount);
	vertexArray.AddIndexComponent(indicesBuffer);
	vertexArray.AddComponent(0, verticesBuffer);

}

void GeometryHelper::GenerateSphere(uint32_t rings, uint32_t segments, uint32_t*& indices, vec3*& vertices,
	uint32_t* indexCount, uint32_t* vertexCount) {

	rings = rings < 3 ? 3 : rings;
	segments = segments < 3 ? 3 : segments;

	uint32_t innerRings = rings - 2;
	*vertexCount = innerRings * segments + 2;
	*indexCount = (innerRings - 1) * segments * 6 + 2 * segments * 3;

	vertices = new vec3[*vertexCount];
	indices = new uint32_t[*indexCount];

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
		indices[indexIndex++] = i + 1;
		indices[indexIndex++] = 0;
		indices[indexIndex++] = (i + 1) % segments + 1;
	}
	
	// Indices for the lower outer ring
	for (uint32_t i = 0; i < segments; i++) {
		indices[indexIndex++] = *vertexCount - 1 - segments + (i + 1) % segments;
		indices[indexIndex++] = *vertexCount - 1;
		indices[indexIndex++] = *vertexCount - 1 - segments + i;
	}
	
	uint32_t offset = 1;
	
	// Generate the indices for the inner rings
	for (uint32_t i = 0; i < innerRings - 1; i++) {
		offset += segments;
		for (uint32_t j = 0; j < segments; j++) {
			indices[indexIndex++] = offset - segments + j;
			indices[indexIndex++] = offset + (j + 1) % segments;
			indices[indexIndex++] = offset + j;
			indices[indexIndex++] = offset - segments + (j + 1) % segments;
			indices[indexIndex++] = offset + (j + 1) % segments;
			indices[indexIndex++] = offset - segments + j;
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