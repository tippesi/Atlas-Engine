#include "VertexArray.h"

uint32_t VertexArray::boundVertexArrayID = 0;

VertexArray::VertexArray() {

	glGenVertexArrays(1, &ID);

	indexComponent = nullptr;

}

void VertexArray::AddIndexComponent(VertexBuffer* buffer) {

	Bind();
	buffer->Bind();

	indexComponent = buffer;

}

void VertexArray::AddComponent(uint32_t attribArray, VertexBuffer* buffer, bool normalized) {

	Bind();
	buffer->Bind();

	glEnableVertexAttribArray(attribArray);
	glVertexAttribPointer(attribArray, buffer->GetStride(), buffer->GetDataType(), normalized, 0, NULL);

	vertexComponents[attribArray] = buffer;

}

void VertexArray::AddInstancedComponent(uint32_t attribArray, VertexBuffer* buffer, bool normalized) {

	Bind();
	buffer->Bind();

	glEnableVertexAttribArray(attribArray);
	glVertexAttribPointer(attribArray, buffer->GetStride(), buffer->GetDataType(), normalized, 0, NULL);
	glVertexAttribDivisor(attribArray, 1);

	vertexComponents[attribArray] = buffer;

}

VertexBuffer* VertexArray::GetIndexComponent() {

	return indexComponent;

}

VertexBuffer* VertexArray::GetComponent(uint32_t attribArray) {

	return vertexComponents[attribArray];

}

void VertexArray::Bind() {

	if (boundVertexArrayID != ID) {

		glBindVertexArray(ID);

		boundVertexArrayID = ID;

	}

}

void VertexArray::Unbind() {

	glBindVertexArray(0);

	boundVertexArrayID = 0;

}

void VertexArray::ClearContent() {

	indexComponent = nullptr;
	vertexComponents.clear();

}

void VertexArray::DeleteContent() {

	delete indexComponent;

	for (auto& bufferKey : vertexComponents) {
		delete bufferKey.second;
	}

	ClearContent();

}

VertexArray::~VertexArray() {

	glDeleteVertexArrays(1, &ID);

}