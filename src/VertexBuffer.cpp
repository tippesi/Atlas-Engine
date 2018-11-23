#include "VertexBuffer.h"

VertexBuffer::VertexBuffer(uint32_t type, int32_t dataType, int32_t stride, uint32_t usage) : 
	type(type), usage(usage), dataType(dataType), stride(stride) {

	glGenBuffers(1, &ID);

	sizeInBytes = 0;

}

void VertexBuffer::SetData(uint8_t* data, int32_t length) {

	SetDataInternal(data, length, sizeof(uint8_t));

}

void VertexBuffer::SetData(uint16_t* data, int32_t length) {

	SetDataInternal(data, length, sizeof(uint16_t));

}

void VertexBuffer::SetData(uint32_t* data, int32_t length) {

	SetDataInternal(data, length, sizeof(uint32_t));

}

void VertexBuffer::SetData(int8_t* data, int32_t length) {

	SetDataInternal(data, length, sizeof(int8_t));

}

void VertexBuffer::SetData(int16_t* data, int32_t length) {

	SetDataInternal(data, length, sizeof(int16_t));

}

void VertexBuffer::SetData(int32_t* data, int32_t length) {

	SetDataInternal(data, length, sizeof(int32_t));

}

void VertexBuffer::SetData(float* data, int32_t length) {

	SetDataInternal(data, length, sizeof(float));

}

void VertexBuffer::SetData(vec2* data, int32_t length) {

	SetDataInternal(glm::value_ptr(data[0]), length, sizeof(vec2));

}

void VertexBuffer::SetData(vec3* data, int32_t length) {

	SetDataInternal(glm::value_ptr(data[0]), length, sizeof(vec3));

}

void VertexBuffer::SetData(vec4* data, int32_t length) {

	SetDataInternal(glm::value_ptr(data[0]), length, sizeof(vec4));

}

void VertexBuffer::SetData(void* data, int32_t length, int32_t elementSize) {

	SetDataInternal(data, length, elementSize);

}

void VertexBuffer::SetSubData(uint8_t* data, int32_t offset, int32_t length) {

	SetSubDataInternal(data, offset, length, sizeof(uint8_t));

}

void VertexBuffer::SetSubData(uint16_t* data, int32_t offset, int32_t length) {

	SetSubDataInternal(data, offset, length, sizeof(uint16_t));

}

void VertexBuffer::SetSubData(uint32_t* data, int32_t offset, int32_t length) {

	SetSubDataInternal(data, offset, length, sizeof(uint32_t));

}

void VertexBuffer::SetSubData(int8_t* data, int32_t offset, int32_t length) {

	SetSubDataInternal(data, offset, length, sizeof(int8_t));

}

void VertexBuffer::SetSubData(int16_t* data, int32_t offset, int32_t length) {

	SetSubDataInternal(data, offset, length, sizeof(int16_t));

}

void VertexBuffer::SetSubData(int32_t* data, int32_t offset, int32_t length) {

	SetSubDataInternal(data, offset, length, sizeof(int32_t));

}

void VertexBuffer::SetSubData(float* data, int32_t offset, int32_t length) {

	SetSubDataInternal(data, offset, length, sizeof(float));

}

void VertexBuffer::SetSubData(vec2* data, int32_t offset, int32_t length) {

	SetSubDataInternal(glm::value_ptr(data[0]), offset, length, sizeof(vec2));

}

void VertexBuffer::SetSubData(vec3* data, int32_t offset, int32_t length) {

	SetSubDataInternal(glm::value_ptr(data[0]), offset, length, sizeof(vec3));

}

void VertexBuffer::SetSubData(vec4* data, int32_t offset, int32_t length) {

	SetSubDataInternal(glm::value_ptr(data[0]), offset, length, sizeof(vec4));

}

void VertexBuffer::SetSubData(void* data, int32_t offset, int32_t length, int32_t elementSize) {

	SetSubDataInternal(data, offset, length, elementSize);

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

int32_t VertexBuffer::GetStride() {

	return stride;

}

void VertexBuffer::SetDataInternal(void* data, int32_t length, int32_t elementSize) {

	Bind();

	sizeInBytes = length * elementSize;

	glBufferData(type, sizeInBytes, data, usage);

}

void VertexBuffer::SetSubDataInternal(void* data, int32_t offset, int32_t length, int32_t elementSize) {

	Bind();

	offset *= elementSize;
	length *= elementSize;

	if (offset + length > sizeInBytes) {
		return;
	}

	glBufferSubData(type, offset, length, data);

}

VertexBuffer::~VertexBuffer() {

	glDeleteBuffers(1, &ID);

}