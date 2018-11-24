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

VertexArray* GeometryHelper::GenerateSphereVertexArray() {

	return nullptr;

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