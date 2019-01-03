#include "IndexBuffer.h"

IndexBuffer::IndexBuffer(uint32_t dataType, size_t elementSize, size_t elementCount) : dataType(dataType),
    Buffer(INDEX_BUFFER, elementSize, BUFFER_IMMUTABLE) {

	SetSize(elementCount);

}

IndexBuffer::~IndexBuffer() {



}

void IndexBuffer::SetData(void *data, size_t offset, size_t length) {

    auto stagingBuffer = Buffer(STAGING_BUFFER, elementSize, BUFFER_DYNAMIC_STORAGE);
    stagingBuffer.SetSize(length);

    // We don't need to bind because it is already bound by previous operations
    stagingBuffer.SetData(data, 0, length);

    Copy(&stagingBuffer, 0, offset * elementSize, length * elementSize);

}

uint32_t IndexBuffer::GetDataType() {

    return dataType;

}