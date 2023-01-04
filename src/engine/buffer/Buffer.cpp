#include "Buffer.h"
#include "graphics/Extensions.h"
#include "graphics/GraphicsDevice.h"

namespace Atlas {

    namespace OldBuffer {

        Buffer::Buffer(BufferUsage bufferUsage, size_t elementSize, size_t elementCount,
            void* data) : usage(bufferUsage), elementSize(elementSize) {

            multiBuffered = bufferUsage & MultiBuffered;
            hostAccessible = bufferUsage & HostAccess;

			if (elementCount) {
				SetSize(elementCount, data);
			}

        }

        Graphics::Buffer *Buffer::Get() {

            if (multiBuffered) {
                return multiBuffer->GetCurrent();
            }
            else {
                return buffer.get();
            }

        }

        void* Buffer::Map() {

            if (!hostAccessible) return nullptr;

            return Get()->Map();

        }

        void Buffer::Unmap() {

            if (!hostAccessible) return;

            Get()->Unmap();

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

            if (multiBuffered) {
                multiBuffer->SetData(data, offset * elementSize, length * elementSize);
            }
            else {
                buffer->SetData(data, offset * elementSize, length * elementSize);
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

        void Buffer::Reallocate(void *data) {

            auto device = Graphics::GraphicsDevice::DefaultDevice;

            VkBufferUsageFlags usageFlags = {};
            if (usage & BufferUsageBits::UniformBuffer) {
                usageFlags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            }
            if (usage & BufferUsageBits::StorageBuffer) {
                usageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            }

            if (usage & BufferUsageBits::MemoryTransfers) {
                usageFlags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            }

            sizeInBytes = elementCount * elementSize;

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


    }

}