#include "Buffer.h"

namespace Atlas {

    namespace Graphics {

        Buffer::Buffer(MemoryManager *memManager, BufferDesc desc) : memoryManager(memManager) {

            VkBufferCreateInfo bufferInfo = {};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = desc.size;
            bufferInfo.usage = desc.usageFlags;

            VmaAllocationCreateInfo allocationCreateInfo = {};
            allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

            VK_CHECK(vmaCreateBuffer(memManager->allocator, &bufferInfo,
                &allocationCreateInfo, &buffer, &allocation, nullptr))

            if (desc.data) SetData(desc.data, 0, desc.size);

        }

        Buffer::~Buffer() {

            memoryManager->DestroyAllocation(BufferAllocation { buffer, allocation });

        }

        void Buffer::SetData(void *data, size_t offset, size_t length) {

            VkBufferCopy bufferCopy = {};
            bufferCopy.srcOffset = 0;
            bufferCopy.dstOffset = 0;
            bufferCopy.size = length;

            memoryManager->uploadManager->UploadBufferData(data, buffer, bufferCopy);

        }

    }

}