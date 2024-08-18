#include "MemoryManager.h"
#include "GraphicsDevice.h"
#include "Instance.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <forward_list>

namespace Atlas {

    namespace Graphics {

        MemoryManager::MemoryManager(GraphicsDevice* device) : device(device) {

            VmaVulkanFunctions vulkanFunctions = {};
            vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
            vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

            VmaAllocatorCreateInfo allocatorInfo = {};
            allocatorInfo.physicalDevice = device->physicalDevice;
            allocatorInfo.device = device->device;
            allocatorInfo.instance = device->instance->GetNativeInstance();
            allocatorInfo.pVulkanFunctions = &vulkanFunctions;
            allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
            allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

            VK_CHECK(vmaCreateAllocator(&allocatorInfo, &allocator))

            VkBufferCreateInfo sampleBufCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
            sampleBufCreateInfo.size = 0x100;
            sampleBufCreateInfo.usage =  
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

            if (device->support.hardwareRayTracing)
                sampleBufCreateInfo.usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;

            VmaAllocationCreateInfo sampleAllocCreateInfo = {};
            sampleAllocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            sampleAllocCreateInfo.priority = 1.0f;

            uint32_t memTypeIndex;
            VkResult res = vmaFindMemoryTypeIndexForBufferInfo(allocator,
                &sampleBufCreateInfo, &sampleAllocCreateInfo, &memTypeIndex);

            VmaPoolCreateInfo poolCreateInfo = {};
            poolCreateInfo.memoryTypeIndex = memTypeIndex;

            VK_CHECK(vmaCreatePool(allocator, &poolCreateInfo, &hightPriorityBufferPool));

            vkGetPhysicalDeviceProperties(device->physicalDevice, &deviceProperties);

            transferManager = new MemoryTransferManager(device, this);

        }

        MemoryManager::~MemoryManager() {

            delete transferManager;

            DestroyAllImmediate();

            vmaDestroyPool(allocator, hightPriorityBufferPool);
            vmaDestroyAllocator(allocator);

        }

        void MemoryManager::DestroyAllocation(Ref<RenderPass>& allocation) {

            deleteRenderPassAllocations.
                emplace_back(DeleteResource<RenderPass> { allocation, frameIndex + framesToDeletion });

        }

        void MemoryManager::DestroyAllocation(Ref<FrameBuffer>& allocation) {

            deleteFrameBufferAllocations.
                emplace_back(DeleteResource<FrameBuffer> { allocation, frameIndex + framesToDeletion });

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

        void MemoryManager::DestroyAllocation(Ref<DescriptorSetLayout>& allocation) {

            deleteDescriptorSetLayoutAllocations
                .emplace_back(DeleteResource<DescriptorSetLayout> { allocation, frameIndex + framesToDeletion });

        }

        void MemoryManager::DestroyAllocation(Ref<DescriptorPool>& allocation) {

            deleteDescriptorPoolAllocations
                .emplace_back(DeleteResource<DescriptorPool> { allocation, frameIndex + framesToDeletion });

        }

        void MemoryManager::DestroyAllocation(Ref<QueryPool>& allocation) {

            deleteQueryPoolAllocations
                .emplace_back(DeleteResource<QueryPool> { allocation, frameIndex + framesToDeletion });

        }

        void MemoryManager::DestroyAllocation(Ref<BLAS>& allocation) {

            deleteBLASAllocations
                .emplace_back(DeleteResource<BLAS> { allocation, frameIndex + framesToDeletion });

        }

        void MemoryManager::DestroyAllocation(Ref<TLAS>& allocation) {

            deleteTLASAllocations
                .emplace_back(DeleteResource<TLAS> { allocation, frameIndex + framesToDeletion });

        }

        void MemoryManager::DestroyRawAllocation(std::function<void()> destroyLambda) {

            deleteRawAllocations.push_back(DeleteLambda { destroyLambda, frameIndex + framesToDeletion } );

        }

        void MemoryManager::DestroyAllImmediate() {

            // Cheap trick to make sure everything is deleted
            frameIndex += framesToDeletion;
            DeleteData();

        }

        void MemoryManager::UpdateFrameIndex(size_t frameIndex) {

            this->frameIndex = frameIndex;

        }

        void MemoryManager::DeleteData() {

            // First delete raw data, mustn't have dependencies
            while (deleteRawAllocations.size() &&
                deleteRawAllocations.front().deleteFrame <= frameIndex) {
                auto &deleteLambda = deleteRawAllocations.front();

                deleteLambda.lambda();

                deleteRawAllocations.pop_front();
            }

            DeleteAllocations(deleteTLASAllocations);
            DeleteAllocations(deleteBLASAllocations);
            DeleteAllocations(deletePipelineAllocations);
            DeleteAllocations(deleteFrameBufferAllocations);
            DeleteAllocations(deleteRenderPassAllocations);
            DeleteAllocations(deleteShaderAllocations);
            DeleteAllocations(deleteBufferAllocations);
            DeleteAllocations(deleteMultiBufferAllocations);
            DeleteAllocations(deleteImageAllocations);
            DeleteAllocations(deleteSamplerAllocations);
            DeleteAllocations(deleteDescriptorPoolAllocations);
            DeleteAllocations(deleteDescriptorSetLayoutAllocations);
            DeleteAllocations(deleteQueryPoolAllocations);

        }

    }

}