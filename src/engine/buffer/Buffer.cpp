#include "Buffer.h"
#include "graphics/Extensions.h"
#include "graphics/GraphicsDevice.h"

namespace Atlas {

    namespace Buffer {

        Buffer::Buffer(BufferUsage bufferUsage, size_t elementSize, size_t elementCount,
            void* data) : usage(bufferUsage), elementSize(elementSize) {

            multiBuffered = bufferUsage & MultiBufferedBit;
            hostAccessible = bufferUsage & HostAccessBit;

			if (elementCount) {
				SetSize(elementCount, data);
			}

        }

        Ref<Graphics::Buffer> Buffer::Get() {

            return buffer;

        }

        Ref<Graphics::MultiBuffer> Buffer::GetMultiBuffer() {

            return multiBuffer;

        }

        void* Buffer::Map() {

            if (!hostAccessible) return nullptr;

            return GetPointer()->Map();

        }

        void Buffer::Unmap() {

            if (!hostAccessible) return;

            GetPointer()->Unmap();

        }

        void Buffer::SetSize(size_t elementCount, void* data) {

            // If the element count is the same we can reuse the old buffer
			if (this->elementCount == elementCount) {
				if (!data)
					return;
				SetData(data, 0, elementCount);
				return;
			}

            this->elementCount = elementCount;
            sizeInBytes = elementCount * elementSize;

            Reallocate(data);

        }

        void Buffer::SetData(void *data, size_t offset, size_t length) {

            if (usage & BufferUsageBits::UniformBufferBit) {
                auto alignedSize = Graphics::Buffer::GetAlignedSize(elementSize);
                // If the buffer is host accessible we can speed the writes up by just mapping once
                if (hostAccessible) {
                    if (multiBuffered) {
                        multiBuffer->Map();
                    } else {
                        buffer->Map();
                    }
                }
                // We need to respect the alignment for uniform buffers, in this case just write
                // to it one by one
                for(size_t i = 0; i < length; i++) {
                    auto elementIdx = i + offset;
                    if (multiBuffered) {
                        multiBuffer->SetData(data, elementIdx * alignedSize, elementSize);
                    } else {
                        buffer->SetData(data, elementIdx * alignedSize, elementSize);
                    }
                }
                // If the buffer is host accessible we can speed the writes up by just mapping once
                if (hostAccessible) {
                    if (multiBuffered) {
                        multiBuffer->Unmap();
                    } else {
                        buffer->Unmap();
                    }
                }
            }
            else {
                if (multiBuffered) {
                    multiBuffer->SetData(data, offset * elementSize, length * elementSize);
                } else {
                    buffer->SetData(data, offset * elementSize, length * elementSize);
                }
            }

        }

        void Buffer::Copy(const Buffer *copyBuffer, size_t readOffset, 
			size_t writeOffset, size_t length) {

			if (!length)
				return;



        }

        uint32_t Buffer::GetUsage() {

            return usage;

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

        size_t Buffer::GetAlignedOffset(size_t elementIndex) {

            return Graphics::Buffer::GetAlignedSize(elementSize) * elementIndex;

        }

        void Buffer::Reallocate(void *data) {

            auto device = Graphics::GraphicsDevice::DefaultDevice;

            sizeInBytes = elementCount * elementSize;

            VkBufferUsageFlags usageFlags = {};
            if (usage & BufferUsageBits::UniformBufferBit) {
                usageFlags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
                // Adjust size for uniform buffers to be aligned. This way we can use
                // them with dynamic offsets
                sizeInBytes = Graphics::Buffer::GetAlignedSize(elementSize) * elementCount;
            }
            if (usage & BufferUsageBits::StorageBufferBit) {
                usageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            }
            if (usage & BufferUsageBits::IndirectBufferBit) {
                usageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
            }

            if (usage & BufferUsageBits::MemoryTransfersBit) {
                usageFlags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            }

            Graphics::BufferDesc desc {
                .usageFlags = usageFlags | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .domain = hostAccessible ? Graphics::BufferDomain::Host : Graphics::BufferDomain::Device,
                .data = data,
                .size = sizeInBytes
            };

            if (multiBuffered) {
                multiBuffer = device->CreateMultiBuffer(desc);
                buffer.reset();
            }
            else {
                buffer = device->CreateBuffer(desc);
                multiBuffer.reset();
            }

        }

        Graphics::Buffer *Buffer::GetPointer() {

            if (multiBuffered) {
                return multiBuffer->GetCurrent();
            }
            else {
                return buffer.get();
            }

        }

    }

}