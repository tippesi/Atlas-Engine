#include "IndexBuffer.h"

#include "../graphics/GraphicsDevice.h"

namespace Atlas {

    namespace Buffer {

        IndexBuffer::IndexBuffer(VkIndexType type, size_t elementCount, void* data) : type(type) {

            switch(type) {
                case VK_INDEX_TYPE_UINT16: elementSize = 2; break;
                case VK_INDEX_TYPE_UINT32: elementSize = 4; break;
                default: elementSize = 4; break;
            }

            if (elementCount) {
                SetSize(elementCount, data);
            }

        }

        void IndexBuffer::SetSize(size_t elementCount, void *data) {

            // If the element count is the same we can reuse the old buffer
            if (this->elementCount == elementCount) {
                if (!data)
                    return;
                SetData(data, 0, elementCount);
                return;
            }

            this->elementCount = elementCount;

            Reallocate(data);

        }

        void IndexBuffer::SetData(void *data, size_t offset, size_t length) {

            buffer->SetData(data, offset * elementSize, length * elementSize);

        }

        void IndexBuffer::Reallocate(void *data) {

            auto device = Graphics::GraphicsDevice::DefaultDevice;

            auto sizeInBytes = elementCount * elementSize;

            Graphics::BufferDesc desc {
                .usageFlags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
                    | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                .domain = Graphics::BufferDomain::Device,
                .data = data,
                .size = sizeInBytes
            };
            buffer = device->CreateBuffer(desc);

        }

    }

}