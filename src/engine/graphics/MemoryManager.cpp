#include "MemoryManager.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

namespace Atlas {

    namespace Graphics {

        MemoryManager::MemoryManager(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device,
            uint32_t transferQueueFamilyIndex, VkQueue transferQueue) : instance(instance),
            physicalDevice(physicalDevice), device(device) {

            VmaVulkanFunctions vulkanFunctions = {};
            vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
            vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

            VmaAllocatorCreateInfo allocatorInfo = {};
            allocatorInfo.physicalDevice = physicalDevice;
            allocatorInfo.device = device;
            allocatorInfo.instance = instance;
            allocatorInfo.pVulkanFunctions = &vulkanFunctions;
            VK_CHECK(vmaCreateAllocator(&allocatorInfo, &allocator))

            vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

            uploadManager = new MemoryUploadManager(this, transferQueueFamilyIndex, transferQueue);

        }

        MemoryManager::~MemoryManager() {

            delete uploadManager;

            // Cheap trick to make sure everything is deleted
            frameIndex += framesToDeletion;
            DeleteData();

            vmaDestroyAllocator(allocator);

        }

        void MemoryManager::DestroyAllocation(BufferAllocation allocation) {

            deleteBufferAllocations.emplace_back(DeleteBufferAllocation { allocation, frameIndex + framesToDeletion });

        }

        void MemoryManager::DestroyAllocation(ImageAllocation allocation) {

            deleteImageAllocations.emplace_back(DeleteImageAllocation { allocation, frameIndex + framesToDeletion });

        }

        void MemoryManager::UpdateFrameIndex(size_t frameIndex) {

            this->frameIndex = frameIndex;

        }

        void MemoryManager::DeleteData() {

            while (deleteBufferAllocations.size() &&
                deleteBufferAllocations.front().deleteFrame <= frameIndex) {
                auto &allocation = deleteBufferAllocations.front();

                vmaDestroyBuffer(allocator, allocation.allocation.buffer, allocation.allocation.allocation);

                deleteBufferAllocations.pop_front();
            }

            while (deleteImageAllocations.size() &&
                deleteImageAllocations.front().deleteFrame <= frameIndex) {
                auto &allocation = deleteImageAllocations.front();

                vmaDestroyImage(allocator, allocation.allocation.image, allocation.allocation.allocation);

                deleteImageAllocations.pop_front();
            }

        }

    }

}