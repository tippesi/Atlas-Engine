#include "Buffer.h"

bool Buffer::immutableStorageSupported = false;

Buffer::Buffer(uint32_t type, size_t elementSize, uint32_t flags) : type(type), elementSize(elementSize) {

    ID = 0;

    dataFlags = 0;
    mapFlags = 0;

    bufferingIndex = 0;

    mappedData = 0;
    mappedDataOffset = 0;

	elementCount = 0;
	sizeInBytes = 0;

    mapped = false;
    immutable = false;
    dynamicStorage = flags & BUFFER_DYNAMIC_STORAGE ? true : false;

    // Check flags.
    if (flags & BUFFER_DOUBLE_BUFFERING) {
        bufferingCount = 2;
    }
    else if (flags & BUFFER_TRIPLE_BUFFERING) {
        bufferingCount = 3;
    }
    else {
        bufferingCount = 1;
    }

    immutable = flags & BUFFER_IMMUTABLE && immutableStorageSupported;

    // Configure mapping and storage flags.
    mapFlags |= (flags & BUFFER_MAP_READ ? GL_MAP_READ_BIT : 0);
    mapFlags |= (flags & BUFFER_MAP_WRITE ? GL_MAP_WRITE_BIT : 0);

    if (flags & BUFFER_DYNAMIC_STORAGE && immutable) {
        dataFlags |= GL_DYNAMIC_STORAGE_BIT_EXT | GL_MAP_COHERENT_BIT_EXT
                | GL_MAP_PERSISTENT_BIT_EXT | mapFlags;
        mapFlags |= GL_MAP_PERSISTENT_BIT_EXT | GL_MAP_COHERENT_BIT_EXT;
    }
    else if (flags & BUFFER_DYNAMIC_STORAGE && !immutable) {
        dataFlags |= GL_STREAM_DRAW;
    }
    else if (!(flags & BUFFER_DYNAMIC_STORAGE) && !immutable) {
        dataFlags |= GL_STATIC_DRAW;
    }

    // Check for alignment issues and add padding if needed



}

Buffer::~Buffer() {

	DestroyInternal();

}

void Buffer::Bind() {

    glBindBuffer(type, ID);

}

void Buffer::BindRange(size_t offset, size_t length) {

    glBindBufferRange(type, 0, ID, offset * elementSize,
            length * elementSize);

}

void Buffer::BindBase(int32_t base) {

	glBindBufferBase(type, base, ID);

}

void Buffer::Unbind() {

    glBindBuffer(type, 0);

}

void Buffer::Map() {

    if (mapped || !dynamicStorage)
        return;

	// Always map the whole range
    void* data = glMapBufferRange(type, 0, sizeInBytes, mapFlags);
    mappedDataOffset = (size_t)((uint8_t*)data);
	mappedData = 0;

    if (immutable)
        bufferLock.LockRange(0, elementCount * elementSize);

    mapped = true;

}

void Buffer::Unmap() {

    if (!mapped)
        return;

    glUnmapBuffer(type);

    mappedData = 0;
    mappedDataOffset = 0;

    mapped = false;

}

void Buffer::Increment() {

    if (mapped && immutable) {
        bufferLock.LockRange(bufferingIndex * elementSize * elementCount,
			elementCount * elementSize);
    }

    bufferingIndex = (bufferingIndex + 1) % bufferingCount;

    mappedData = bufferingIndex * elementSize * elementCount;

    if (mapped && immutable) {
        bufferLock.WaitForLockedRange(mappedData, elementCount * elementSize);
    }

}

inline int32_t Buffer::GetIncrement() {

    return (int32_t)bufferingIndex;

}

void Buffer::SetSize(size_t elementCount) {

    this->elementCount = elementCount;
    sizeInBytes = elementCount * elementSize * bufferingCount;

    if (!ID) {
        CreateInternal();
        return;
    }

    if (immutable) {
        DestroyInternal();
        CreateInternal();
    }
    else {
        Bind();
        glBufferData(type, bufferingCount * elementCount * elementSize, nullptr, dataFlags);
    }

}

void Buffer::SetData(void *data, size_t offset, size_t length) {

    if (mapped || !dynamicStorage)
        return;

	Bind();

    size_t dataOffset = offset * elementSize +
            bufferingIndex * elementCount * elementSize;

    if (dataOffset + length * elementSize > sizeInBytes)
        return;

    glBufferSubData(type, dataOffset, length * elementSize, data);

}

void Buffer::SetDataMapped(void *data, size_t length) {

    if (!mapped || !dynamicStorage)
        return;

    size_t copyLength = length * elementSize;

    if (sizeInBytes < mappedData + copyLength)
        return;

    void* bufferData = (uint8_t*)(mappedDataOffset + mappedData);

    memcpy(bufferData, data, copyLength);

    mappedData = mappedData + copyLength;

}

void Buffer::Copy(Buffer *copyBuffer, size_t readOffset, size_t writeOffset, size_t length) {

	// The type of the buffer can't be the same because we have to bind both.
    if (copyBuffer->GetType() == type)
        return;

    copyBuffer->Bind();
    Bind();

    glCopyBufferSubData(copyBuffer->GetType(), type, readOffset, writeOffset, length);

}

uint32_t Buffer::GetType() {

    return type;

}

size_t Buffer::GetElementCount() {

    return elementCount;

}

size_t Buffer::GetElementSize() {

    return elementSize;

}

size_t Buffer::GetSize() {

    return sizeInBytes;

}

void Buffer::CheckExtensions() {

    int32_t extensionCount = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &extensionCount);

    for (int32_t i = 0; i < extensionCount; i++) {
        const char* extension = (const char*)glGetStringi(GL_EXTENSIONS, i);
        if (strcmp(extension, "GL_ARB_buffer_storage") == 0) {
            immutableStorageSupported = true;
        }
        if (strcmp(extension, "GL_EXT_buffer_storage") == 0) {
            immutableStorageSupported = true;
        }
    }

}

bool Buffer::IsImmutableStorageSupported() {

    return immutableStorageSupported;

}

void Buffer::CreateInternal() {

    glGenBuffers(1, &ID);

    Bind();

    if (immutable) {
        glBufferStorage(type, sizeInBytes, nullptr, dataFlags);
    }
    else {
        glBufferData(type, sizeInBytes, nullptr, dataFlags);
    }

}

void Buffer::DestroyInternal() {

    Bind();

    if (mapped)
        Unmap();

    glDeleteBuffers(1, &ID);

    ID = 0;

}