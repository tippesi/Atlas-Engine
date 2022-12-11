#include "MemoryManager.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

namespace Atlas {

    namespace Graphics {

        MemoryManager::MemoryManager(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device)
            : instance(instance), physicalDevice(physicalDevice), device(device) {

            VmaVulkanFunctions vulkanFunctions = {};
            vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
            vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

            VmaAllocatorCreateInfo allocatorInfo = {};
            allocatorInfo.physicalDevice = physicalDevice;
            allocatorInfo.device = device;
            allocatorInfo.instance = instance;
            allocatorInfo.pVulkanFunctions = &vulkanFunctions;
            VK_CHECK(vmaCreateAllocator(&allocatorInfo, &allocator))

        }

        MemoryManager::~MemoryManager() {

            vmaDestroyAllocator(allocator);

        }

        void MemoryManager::DestroyAllocation(BufferAllocation allocation) {

            deleteBufferAllocations.emplace_back(DeleteBufferAllocation { allocation, frameIndex + framesToDeletion });

        }

        void MemoryManager::DestroyAllocation(ImageAllocation allocation) {

            deleteImageAllocations.emplace_back(DeleteImageAllocation { allocation, frameIndex  +framesToDeletion });

        }

        void MemoryManager::UpdateFrameIndex(size_t frameIndex) {

            this->frameIndex = frameIndex;

        }

        void MemoryManager::DeleteData() {

            if (deleteBufferAllocations.size()) {
                while (deleteBufferAllocations.front().deleteFrame <= frameIndex) {
                    auto &allocation = deleteBufferAllocations.front();


                    deleteBufferAllocations.pop_front();
                }
            }

        }

    }

}