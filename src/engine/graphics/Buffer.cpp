#include "Buffer.h"
#include "GraphicsDevice.h"

#include <cstring>

namespace Atlas {

    namespace Graphics {

        Buffer::Buffer(GraphicsDevice *device, BufferDesc& desc) : usageFlags(desc.usageFlags),
            domain(desc.domain), size(desc.size), memoryManager(device->memoryManager) {

            VkBufferCreateInfo bufferInfo = {};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = desc.size;
            bufferInfo.usage = desc.usageFlags;

            VmaAllocationCreateInfo allocationCreateInfo = {};
            if (desc.domain == BufferDomain::Host) {
                allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
                allocationCreateInfo.flags = desc.hostAccess == BufferHostAccess::Random ?
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT :
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            }
            else {
                allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            }

            VK_CHECK(vmaCreateBuffer(memoryManager->allocator, &bufferInfo,
                &allocationCreateInfo, &buffer, &allocation, nullptr))

            if (desc.data) SetData(desc.data, 0, desc.size);

        }

        Buffer::~Buffer() {

            vmaDestroyBuffer(memoryManager->allocator, buffer, allocation);

        }

        void Buffer::SetData(void *data, size_t offset, size_t length) {

            // Upload data through staging buffer for device local memory
            if (domain == BufferDomain::Device) {
                VkBufferCopy bufferCopy = {};
                bufferCopy.srcOffset = 0;
                bufferCopy.dstOffset = 0;
                bufferCopy.size = length;

                memoryManager->transferManager->UploadBufferData(data, this, bufferCopy);
            }
            else {
                // If there isn't a valid mapping yet start and complete it in this call
                bool needsMapping = !isMapped;
                if (needsMapping) Map();

                void* offsetAddress = static_cast<uint8_t*>(mappedData) + offset;
                std::memcpy(offsetAddress, data, length);

                if (needsMapping) Unmap();
            }

        }

        void* Buffer::Map() {

            VK_CHECK(vmaMapMemory(memoryManager->allocator, allocation, &mappedData))
            isMapped = true;

            return mappedData;

        }

        void Buffer::Unmap() {

            vmaUnmapMemory(memoryManager->allocator, allocation);
            mappedData = nullptr;
            isMapped = false;

        }

        MultiBuffer::MultiBuffer(GraphicsDevice* device, BufferDesc& desc) : size(desc.size) {

            for (uint32_t i = 0; i < FRAME_DATA_COUNT; i++) {
                frameBuffer[i] = new Buffer(device, desc);
            }

        }

        MultiBuffer::~MultiBuffer() {

            for (uint32_t i = 0; i < FRAME_DATA_COUNT; i++) {
                delete frameBuffer[i];
            }

        }

        void MultiBuffer::SetData(void *data, size_t offset, size_t length) {

            GetCurrent()->SetData(data, offset, length);

        }

        Buffer* MultiBuffer::GetCurrent() const {

            return frameBuffer[frameIndex % FRAME_DATA_COUNT];

        }

        void MultiBuffer::UpdateFrameIndex(size_t frameIndex) {

            this->frameIndex = frameIndex;

        }

    }

}