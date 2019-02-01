#include "VertexBuffer.h"

VertexBuffer::VertexBuffer(uint32_t dataType, int32_t stride, size_t elementSize, size_t elementCount, uint32_t flags) :
	Buffer(AE_VERTEX_BUFFER, elementSize, flags), dataType(dataType), stride(stride) {

	SetSize(elementCount);

}

VertexBuffer::~VertexBuffer() {



}

void VertexBuffer::SetData(void *data, size_t offset, size_t length) {

	auto stagingBuffer = Buffer(AE_STAGING_BUFFER, elementSize, AE_BUFFER_DYNAMIC_STORAGE);
	stagingBuffer.SetSize(length);

	// We don't need to bind because it is already bound by previous operations
	stagingBuffer.SetData(data, 0, length);

	Copy(&stagingBuffer, 0, offset * elementSize, length * elementSize);

}

uint32_t VertexBuffer::GetDataType() {

	return dataType;

}

int32_t VertexBuffer::GetStride() {

	return stride;

}