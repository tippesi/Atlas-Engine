#include "Descriptor.h"

namespace Atlas {

    namespace Graphics {

        DescriptorPool::DescriptorPool(MemoryManager *memManager) : memoryManager(memManager) {

            pools.push_back(InitPool());

        }

        DescriptorPool::~DescriptorPool() {

            for (auto pool : pools) {
                vkDestroyDescriptorPool(memoryManager->device, pool, nullptr);
            }

        }

        DescriptorAllocation DescriptorPool::Allocate(const DescriptorAllocationDesc &desc) {

            DescriptorAllocation allocation;



        }

        void DescriptorPool::Reset() {

            for (auto pool : pools) {
                vkResetDescriptorPool(memoryManager->device, pool, 0);
            }

        }

        VkDescriptorPool DescriptorPool::InitPool() {

            std::vector<VkDescriptorPoolSize> sizes = {
                    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, DESCRIPTOR_POOL_SIZE },
                    { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          DESCRIPTOR_POOL_SIZE },
                    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         DESCRIPTOR_POOL_SIZE },
                    { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          DESCRIPTOR_POOL_SIZE },
                    { VK_DESCRIPTOR_TYPE_SAMPLER,                DESCRIPTOR_POOL_SIZE },
                };

            VkDescriptorPoolCreateInfo poolInfo = {};
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolInfo.flags = 0;
            poolInfo.maxSets = sizes.size() * DESCRIPTOR_POOL_SIZE;
            poolInfo.poolSizeCount = uint32_t(sizes.size());
            poolInfo.pPoolSizes = sizes.data();

            VkDescriptorPool pool;
            VK_CHECK(vkCreateDescriptorPool(memoryManager->device, &poolInfo, nullptr, &pool))

            return pool;

        }

    }

}