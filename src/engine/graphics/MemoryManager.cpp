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
            VK_CHECK(vmaCreateAllocator(&allocatorInfo, &allocator))

            vkGetPhysicalDeviceProperties(device->physicalDevice, &deviceProperties);

            transferManager = new MemoryTransferManager(device, this);

        }

        MemoryManager::~MemoryManager() {

            delete transferManager;

            DestroyAllImmediate();

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

        void MemoryManager::DestroyAllocation(Ref<DescriptorPool>& allocation) {

            deleteDescriptorPoolAllocations
                .emplace_back(DeleteResource<DescriptorPool> { allocation, frameIndex + framesToDeletion });

        }

        void MemoryManager::DestroyAllocation(Ref<QueryPool>& allocation) {

            deleteQueryPoolAllocations
                .emplace_back(DeleteResource<QueryPool> { allocation, frameIndex + framesToDeletion });

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

            DeleteAllocations(deletePipelineAllocations);
            DeleteAllocations(deleteFrameBufferAllocations);
            DeleteAllocations(deleteRenderPassAllocations);
            DeleteAllocations(deleteShaderAllocations);
            DeleteAllocations(deleteBufferAllocations);
            DeleteAllocations(deleteMultiBufferAllocations);
            DeleteAllocations(deleteImageAllocations);
            DeleteAllocations(deleteSamplerAllocations);
            DeleteAllocations(deleteDescriptorPoolAllocations);
            DeleteAllocations(deleteQueryPoolAllocations);

        }

    }

}