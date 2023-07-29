#include "Buffer.h"
#include "GraphicsDevice.h"

#include <cstring>

namespace Atlas {

    namespace Graphics {

        Buffer::Buffer(GraphicsDevice *device, const BufferDesc& desc) : usageFlags(desc.usageFlags),
            domain(desc.domain), size(desc.size), alignment(desc.alignment), memoryManager(device->memoryManager) {

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

            if (alignment == 0) {
                VK_CHECK(vmaCreateBuffer(memoryManager->allocator, &bufferInfo,
                    &allocationCreateInfo, &buffer, &allocation, nullptr))
            }
            else {
                VK_CHECK(vmaCreateBufferWithAlignment(memoryManager->allocator, &bufferInfo,
                    &allocationCreateInfo, alignment, &buffer, &allocation, nullptr))
            }

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

        VkDeviceAddress Buffer::GetDeviceAddress() {

            VkBufferDeviceAddressInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            info.buffer = buffer;
            
            return vkGetBufferDeviceAddress(memoryManager->device->device, &info);

        }

        size_t Buffer::GetAlignedSize(size_t size) {

            auto device = GraphicsDevice::DefaultDevice;

            size_t minUboAlignment = device->deviceProperties.properties.limits.minUniformBufferOffsetAlignment;
            size_t alignedSize = size;
            if (minUboAlignment > 0) {
                alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
            }
            return alignedSize;

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

        void* MultiBuffer::Map() {

            return GetCurrent()->Map();

        }

        void MultiBuffer::Unmap() {

            GetCurrent()->Unmap();

        }

        Buffer* MultiBuffer::GetCurrent() const {

            return frameBuffer[frameIndex % FRAME_DATA_COUNT];

        }

        void MultiBuffer::UpdateFrameIndex(size_t frameIndex) {

            this->frameIndex = frameIndex;

        }

    }

}