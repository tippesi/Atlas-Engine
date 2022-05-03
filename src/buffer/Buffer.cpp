#include "Buffer.h"
#include "../TypeFormat.h"
#include "../texture/TextureFormat.h"
#include "../Extensions.h"

namespace Atlas {

    namespace Buffer {

        Buffer::Buffer(const Buffer& that) {

			DeepCopy(that);

        }

        Buffer::Buffer(uint32_t type, size_t elementSize, uint32_t flags,
			size_t elementCount, void* data) : type(type), elementSize(elementSize),
			flags(flags) {

            dynamicStorage = flags & AE_BUFFER_DYNAMIC_STORAGE;

            // Check flags.
            if (flags & AE_BUFFER_DOUBLE_BUFFERING) {
                bufferingCount = 2;
            }
            else if (flags & AE_BUFFER_TRIPLE_BUFFERING) {
                bufferingCount = 3;
            }
            else {
                bufferingCount = 1;
            }

            immutable = (flags & AE_BUFFER_IMMUTABLE);

            // Configure mapping and storage flags.
            mapFlags |= ((flags & AE_BUFFER_MAP_READ) ? GL_MAP_READ_BIT : 0);
            mapFlags |= ((flags & AE_BUFFER_MAP_WRITE) ? GL_MAP_WRITE_BIT : 0);

            if (flags & AE_BUFFER_DYNAMIC_STORAGE && immutable) {
                dataFlags |= GL_DYNAMIC_STORAGE_BIT_EXT | GL_MAP_COHERENT_BIT_EXT
                             | GL_MAP_PERSISTENT_BIT_EXT | mapFlags;
                mapFlags |= GL_MAP_PERSISTENT_BIT_EXT | GL_MAP_COHERENT_BIT_EXT;
            }
            else if (flags & AE_BUFFER_DYNAMIC_STORAGE && !immutable) {
                dataFlags |= GL_STREAM_DRAW;
            }
            else if (!(flags & AE_BUFFER_DYNAMIC_STORAGE) && !immutable) {
                dataFlags |= GL_STATIC_DRAW;
            }

			if (elementCount) {
				SetSize(elementCount, data);
			}

        }

        Buffer::~Buffer() {

            DestroyInternal();

        }

        Buffer& Buffer::operator=(const Buffer &that) {

            if (this != &that) {

				DeepCopy(that);

            }

            return *this;

        }

        void Buffer::Bind() const {

            glBindBuffer(type, ID);

        }

        void Buffer::BindAs(uint32_t target) const {

            glBindBuffer(target, ID);

        }

        void Buffer::BindRange(size_t offset, size_t length, int32_t base) const {

            glBindBufferRange(type, base, ID, offset * elementSize,
                              length * elementSize);

        }

        void Buffer::BindBase(int32_t base) const {

            glBindBufferBase(type, base, ID);

        }

        void Buffer::BindBaseAs(uint32_t target, int32_t base) const {

            glBindBufferBase(target, base, ID);

        }

        void Buffer::Unbind() const {

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

        void Buffer::SetSize(size_t elementCount, void* data) {

			if (this->elementCount == elementCount) {
				if (!data)
					return;
				SetData(data, 0, elementCount);
				return;
			}

            this->elementCount = elementCount;
            sizeInBytes = elementCount * elementSize * bufferingCount;

            if (!ID) {
                CreateInternal(data);
                return;
            }

            if (immutable) {
                DestroyInternal();
                CreateInternal(data);
            }
            else {
                Bind();
                glBufferData(type, sizeInBytes, data, dataFlags);
            }

        }

        void Buffer::SetData(void *data, size_t offset, size_t length) {

            if (mapped || (!dynamicStorage && immutable) || !length || !data)
                return;

            Bind();

            size_t dataOffset = offset * elementSize +
                                bufferingIndex * elementCount * elementSize;

            if (dataOffset + length * elementSize > sizeInBytes)
                return;

            glBufferSubData(type, dataOffset, length * elementSize, data);

        }

        void Buffer::SetDataMapped(void *data, size_t length) {

            if (!mapped || !dynamicStorage || !length || !data)
                return;

            size_t copyLength = length * elementSize;

            if (sizeInBytes < mappedData + copyLength)
                return;

            void* bufferData = (uint8_t*)(mappedDataOffset + mappedData);

            std::memcpy(bufferData, data, copyLength);

            mappedData = mappedData + copyLength;

        }

        int32_t Buffer::GetDataMappedAdvancement() {

            return (int32_t)(mappedData / elementSize);

        }

		void Buffer::InvalidateData() {

			glInvalidateBufferData(ID);

		}

		void Buffer::ClearData(int32_t sizedFormat, int32_t type, void* data) {

			glClearBufferData(this->type, sizedFormat,
				Texture::TextureFormat::GetBaseFormat(sizedFormat),
				type, data);

		}

        void Buffer::Copy(const Buffer *copyBuffer, size_t readOffset, 
			size_t writeOffset, size_t length) {

			if (!length)
				return;

			glBindBuffer(GL_COPY_WRITE_BUFFER, ID);
			glBindBuffer(GL_COPY_READ_BUFFER, copyBuffer->ID);

            glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 
				readOffset, writeOffset, length);

			glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
			glBindBuffer(GL_COPY_READ_BUFFER, 0);

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

		void Buffer::DeepCopy(const Buffer& that) {

			if (mapped && !immutable)
				Unmap();

			type = that.type;

			elementSize = that.elementSize;

			mapFlags = that.mapFlags;
			dataFlags = that.dataFlags;
			
			immutable = that.immutable;
			dynamicStorage = that.dynamicStorage;

			bufferingCount = that.bufferingCount;
			bufferingIndex = 0;

			SetSize(that.elementCount);
			Copy(&that, 0, 0, that.sizeInBytes);

		}

        void Buffer::CreateInternal(void* data) {

            glGenBuffers(1, &ID);

            Bind();

            if (immutable) {
#ifndef AE_OS_ANDROID
#ifdef AE_API_GLES
                glBufferStorageEXT(type, sizeInBytes, data, dataFlags);
#else
                glBufferStorage(type, sizeInBytes, data, dataFlags);
#endif
#endif
            }
            else {
                glBufferData(type, sizeInBytes, data, dataFlags);
            }

        }

        void Buffer::DestroyInternal() {

            Bind();

            if (mapped)
                Unmap();

            glDeleteBuffers(1, &ID);

            ID = 0;

        }

    }

}