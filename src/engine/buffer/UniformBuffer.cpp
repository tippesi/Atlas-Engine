#include "UniformBuffer.h"

#include "../graphics/GraphicsDevice.h"

namespace Atlas {

    namespace Buffer {

        UniformBuffer::UniformBuffer(size_t elementSize, size_t elementCount, bool hostAccess)
            : elementCount(elementCount), elementSize(elementSize), hostAccess(hostAccess) {

            Reallocate(nullptr);

        }

        void UniformBuffer::Bind(Graphics::CommandList* commandList, uint32_t set, uint32_t binding) {

            commandList->BindBuffer(buffer, set, binding);

        }

        void UniformBuffer::SetSize(size_t elementCount, void *data) {

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

        void UniformBuffer::SetData(void *data, size_t offset, size_t length) {

            buffer->SetData(data, offset * elementSize, length * elementSize);

        }

        Ref<Graphics::MultiBuffer> UniformBuffer::Get() {

            return buffer;

        }

        size_t UniformBuffer::GetElementCount() {

            return elementCount;

        }

        size_t UniformBuffer::GetElementSize() {

            return elementSize;

        }

        size_t UniformBuffer::GetSize() {

            return elementSize * elementCount;

        }

        size_t UniformBuffer::GetAlignedOffset(size_t elementIndex) {

            return Graphics::Buffer::GetAlignedSize(elementSize) * elementIndex;

        }

        void UniformBuffer::Reallocate(void *data) {

            auto device = Graphics::GraphicsDevice::DefaultDevice;

            auto sizeInBytes = elementCount * elementSize;

            Graphics::BufferDesc desc {
                .usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .domain = hostAccess ? Graphics::BufferDomain::Host : Graphics::BufferDomain::Device,
                .data = data,
                .size = sizeInBytes
            };
            buffer = device->CreateMultiBuffer(desc);

        }

    }

}