#include "VertexBuffer.h"

VertexBuffer::VertexBuffer(uint32_t type, uint32_t usage) : type(type), usage(usage) {

	glGenBuffers(1, &ID);

	dataType = 0;
	vertexSize = 0;
	stride = 0;

	sizeInBytes = 0;

}

void VertexBuffer::SetData(uint8_t* data, int32_t length) {

	SetDataInternal(data, length, sizeof(uint8_t), GL_UNSIGNED_BYTE, 1);

}

void VertexBuffer::SetData(uint16_t* data, int32_t length) {

	SetDataInternal(data, length, sizeof(uint16_t), GL_UNSIGNED_SHORT, 1);

}

void VertexBuffer::SetData(uint32_t* data, int32_t length) {

	SetDataInternal(data, length, sizeof(uint32_t), GL_UNSIGNED_INT, 1);

}

void VertexBuffer::SetData(float* data, int32_t length) {

	SetDataInternal(data, length, sizeof(float), GL_FLOAT, 1);

}

void VertexBuffer::SetData(vec2* data, int32_t length) {

	SetDataInternal(glm::value_ptr(data[0]), length, sizeof(vec2), GL_FLOAT, 2);

}

void VertexBuffer::SetData(vec3* data, int32_t length) {

	SetDataInternal(glm::value_ptr(data[0]), length, sizeof(vec3), GL_FLOAT, 3);

}

void VertexBuffer::SetData(vec4* data, int32_t length) {

	SetDataInternal(glm::value_ptr(data[0]), length, sizeof(vec4), GL_FLOAT, 4);

}

void VertexBuffer::SetData(void* data, int32_t length, int32_t vertexSize, int32_t dataType, int32_t stride) {

	SetDataInternal(data, length, vertexSize, dataType, stride);

}

void VertexBuffer::SetSubData(uint8_t* data, int32_t offset, int32_t length) {

	SetSubDataInternal(data, offset, length, sizeof(uint8_t), GL_UNSIGNED_BYTE, 1);

}

void VertexBuffer::SetSubData(uint16_t* data, int32_t offset, int32_t length) {

	SetSubDataInternal(data, offset, length, sizeof(uint16_t), GL_UNSIGNED_SHORT, 1);

}

void VertexBuffer::SetSubData(uint32_t* data, int32_t offset, int32_t length) {

	SetSubDataInternal(data, offset, length, sizeof(uint32_t), GL_UNSIGNED_INT, 1);

}

void VertexBuffer::SetSubData(float* data, int32_t offset, int32_t length) {

	SetSubDataInternal(data, offset, length, sizeof(float), GL_FLOAT, 1);

}

void VertexBuffer::SetSubData(vec2* data, int32_t offset, int32_t length) {

	SetSubDataInternal(glm::value_ptr(data[0]), offset, length, sizeof(vec2), GL_FLOAT, 2);

}

void VertexBuffer::SetSubData(vec3* data, int32_t offset, int32_t length) {

	SetSubDataInternal(glm::value_ptr(data[0]), offset, length, sizeof(vec3), GL_FLOAT, 3);

}

void VertexBuffer::SetSubData(vec4* data, int32_t offset, int32_t length) {

	SetSubDataInternal(glm::value_ptr(data[0]), offset, length, sizeof(vec4), GL_FLOAT, 4);

}

void VertexBuffer::SetSubData(void* data, int32_t offset, int32_t length, int32_t vertexSize, int32_t dataType, int32_t stride) {

	SetSubDataInternal(data, offset, length, vertexSize, dataType, stride);

}


void VertexBuffer::Bind() {

	glBindBuffer(type, ID);

}

void VertexBuffer::Unbind() {

	glBindBuffer(type, 0);

}

uint32_t VertexBuffer::GetType() {

	return type;

}

int32_t VertexBuffer::GetDataType() {

	return dataType;

}

int32_t VertexBuffer::GetVertexSize() {

	return vertexSize;

}

int32_t VertexBuffer::GetStride() {

	return stride;

}

void VertexBuffer::SetDataInternal(void* data, int32_t length, int32_t vertexSize, int32_t dataType, int32_t stride) {

	Bind();

	this->vertexSize = vertexSize;
	this->dataType = dataType;
	this->stride = stride;

	sizeInBytes = length * vertexSize;

	glBufferData(type, sizeInBytes, data, usage);

}

void VertexBuffer::SetSubDataInternal(void* data, int32_t offset, int32_t length, int32_t vertexSize, int32_t dataType, int32_t stride) {

	Bind();

	this->vertexSize = vertexSize;
	this->dataType = dataType;
	this->stride = stride;

	offset *= vertexSize;
	length *= vertexSize;

	if (offset + length > sizeInBytes) {
		return;
	}

	glBufferSubData(type, offset, length, data);

}