#include "MemoryUploadManager.h"
#include "MemoryManager.h"

#include <cstring>

namespace Atlas {

    namespace Graphics {

        MemoryUploadManager::MemoryUploadManager(Atlas::Graphics::MemoryManager *memManager,
            uint32_t transferQueueFamilyIndex, VkQueue transferQueue) : memoryManager(memManager),
            transferQueueFamilyIndex(transferQueueFamilyIndex), transferQueue(transferQueue) {

            auto& device = memManager->device;

            VkCommandPoolCreateInfo poolCreateInfo = Initializers::InitCommandPoolCreateInfo(transferQueueFamilyIndex);
            VK_CHECK(vkCreateCommandPool(device, &poolCreateInfo, nullptr, &commandPool))

            VkCommandBufferAllocateInfo bufferAllocateInfo = Initializers::InitCommandBufferAllocateInfo(commandPool, 1);
            VK_CHECK(vkAllocateCommandBuffers(device, &bufferAllocateInfo, &commandBuffer))

            VkFenceCreateInfo fenceInfo = Initializers::InitFenceCreateInfo();
            VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &fence))

        }

        MemoryUploadManager::~MemoryUploadManager() {

            vkDestroyFence(memoryManager->device, fence, nullptr);
            vkDestroyCommandPool(memoryManager->device, commandPool, nullptr);

        }

        void MemoryUploadManager::UploadBufferData(void *data, VkBuffer destinationBuffer,
            VkBufferCopy bufferCopyDesc) {

            VkDevice device = memoryManager->device;
            VmaAllocator allocator = memoryManager->allocator;

            auto stagingAllocation = CreateStagingBuffer(bufferCopyDesc.size);

            void* destination;
            vmaMapMemory(allocator, stagingAllocation.allocation, &destination);
            std::memcpy(destination, data, bufferCopyDesc.size);
            vmaUnmapMemory(allocator, stagingAllocation.allocation);

            VkCommandBufferBeginInfo cmdBeginInfo =
                Initializers::InitCommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
            VK_CHECK(vkBeginCommandBuffer(commandBuffer, &cmdBeginInfo));

            vkCmdCopyBuffer(commandBuffer, stagingAllocation.buffer, destinationBuffer, 1, &bufferCopyDesc);

            VK_CHECK(vkEndCommandBuffer(commandBuffer));

            VkSubmitInfo submit = Initializers::InitSubmitInfo(&commandBuffer);
            VK_CHECK(vkQueueSubmit(transferQueue, 1, &submit, fence));

            // We wait here until the operation is finished
            vkWaitForFences(device, 1, &fence, true, 9999999999);
            vkResetFences(device, 1, &fence);

            vkResetCommandPool(device, commandPool, 0);
            DestroyStagingBuffer(stagingAllocation);

        }

        MemoryUploadManager::StagingBufferAllocation MemoryUploadManager::CreateStagingBuffer(size_t size) {

            VkBufferCreateInfo stagingBufferInfo = {};
            stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            stagingBufferInfo.pNext = nullptr;

            stagingBufferInfo.size = size;
            stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

            // We want to use memcpy on the buffer, therefore we need the sequential write flag
            VmaAllocationCreateInfo allocationCreateInfo = {};
            allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

            StagingBufferAllocation allocation;
            VK_CHECK(vmaCreateBuffer(memoryManager->allocator, &stagingBufferInfo, &allocationCreateInfo,
                &allocation.buffer, &allocation.allocation, nullptr));

            return allocation;

        }

        void MemoryUploadManager::DestroyStagingBuffer(StagingBufferAllocation &allocation) {

            vmaDestroyBuffer(memoryManager->allocator, allocation.buffer, allocation.allocation);

        }

    }

}