#include "MemoryManager.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <forward_list>

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

            transferManager = new MemoryTransferManager(this, transferQueueFamilyIndex, transferQueue);

        }

        MemoryManager::~MemoryManager() {

            delete transferManager;

            // Cheap trick to make sure everything is deleted
            frameIndex += framesToDeletion;
            DeleteData();

            vmaDestroyAllocator(allocator);

        }

        void MemoryManager::DestroyAllocation(Ref<Shader>& allocation) {

            deleteShaderAllocations.
                emplace_back(DeleteResource<Shader> { allocation, frameIndex + framesToDeletion });

        }

        void MemoryManager::DestroyAllocation(Ref<Pipeline>& allocation) {

            deletePipelineAllocations.
                emplace_back(DeleteResource<Pipeline> { allocation, frameIndex + framesToDeletion });

        }

        void MemoryManager::DestroyAllocation(Ref<Buffer>& allocation) {

            deleteBufferAllocations.
                emplace_back(DeleteResource<Buffer> { allocation, frameIndex + framesToDeletion });

        }

        void MemoryManager::DestroyAllocation(Ref<MultiBuffer>& allocation) {

            deleteMultiBufferAllocations.emplace_back(
                DeleteResource<MultiBuffer> { allocation, frameIndex + framesToDeletion });

        }

        void MemoryManager::DestroyAllocation(Ref<Image>& allocation) {

            deleteImageAllocations
                .emplace_back(DeleteResource<Image> { allocation, frameIndex + framesToDeletion });

        }

        void MemoryManager::DestroyAllocation(Ref<Sampler>& allocation) {

            deleteSamplerAllocations
                .emplace_back(DeleteResource<Sampler> { allocation, frameIndex + framesToDeletion });

        }

        void MemoryManager::DestroyAllocation(Ref<DescriptorPool>& allocation) {

            deleteDescriptorPoolAllocations
                .emplace_back(DeleteResource<DescriptorPool> { allocation, frameIndex + framesToDeletion });

        }

        void MemoryManager::UpdateFrameIndex(size_t frameIndex) {

            this->frameIndex = frameIndex;

        }

        void MemoryManager::DeleteData() {

            DeleteAllocations(deletePipelineAllocations);
            DeleteAllocations(deleteShaderAllocations);
            DeleteAllocations(deleteBufferAllocations);
            DeleteAllocations(deleteMultiBufferAllocations);
            DeleteAllocations(deleteImageAllocations);
            DeleteAllocations(deleteSamplerAllocations);
            DeleteAllocations(deleteDescriptorPoolAllocations);

        }

    }

}