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

    }

}