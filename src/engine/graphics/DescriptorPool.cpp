#include "DescriptorPool.h"
#include "GraphicsDevice.h"

namespace Atlas {

    namespace Graphics {

        DescriptorPool::DescriptorPool(GraphicsDevice* device) : device(device) {

            pools.push_back(InitPool());

        }

        DescriptorPool::~DescriptorPool() {

            for (auto pool : pools) {
                vkDestroyDescriptorPool(device->device, pool, nullptr);
            }

        }

        VkDescriptorSet DescriptorPool::GetCachedSet(VkDescriptorSetLayout layout) {

            // This approach might lead to memory issues. Need to release
            // the cached descriptors at some point
            auto it = layoutAllocationsMap.find(layout);
            if (it == layoutAllocationsMap.end()) {
                layoutAllocationsMap[layout] = LayoutAllocations{};
                it = layoutAllocationsMap.find(layout);
            }

            auto& layoutAllocations = it->second;
            if (layoutAllocations.counter == layoutAllocations.sets.size()) {
                layoutAllocations.sets.push_back(Allocate(layout));
            }

            return layoutAllocations.sets[layoutAllocations.counter++];

        }

        VkDescriptorSet DescriptorPool::Allocate(VkDescriptorSetLayout layout) {

            VkDescriptorSetAllocateInfo allocInfo = {};
            allocInfo.pNext = nullptr;
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = pools[poolIdx];
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &layout;

            VkDescriptorSet set;
            auto result = vkAllocateDescriptorSets(device->device, &allocInfo, &set);
            // Handle the pool out of memory error by allocating a new pool
            if (result == VK_ERROR_OUT_OF_POOL_MEMORY) {
                poolIdx++;
                if (poolIdx == pools.size()) {
                    pools.push_back(InitPool());
                }
                allocInfo.descriptorPool = pools[poolIdx];
                VK_CHECK(vkAllocateDescriptorSets(device->device, &allocInfo, &set))
            } else {
                VK_CHECK(result);
            }

            return set;

        }

        void DescriptorPool::Reset() {

            for (auto& pool : pools) {
                vkResetDescriptorPool(device->device, pool, 0);
            }

            poolIdx = 0;

        }

        void DescriptorPool::ResetAllocationCounters() {

            for (auto& [_, allocations] : layoutAllocationsMap) {
                allocations.counter = 0;
            }

        }

        VkDescriptorPool DescriptorPool::GetNativePool() {

            return pools[poolIdx];

        }

        VkDescriptorPool DescriptorPool::InitPool() {

            std::vector<VkDescriptorPoolSize> sizes = {
                    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, DESCRIPTOR_POOL_SIZE },
                    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, DESCRIPTOR_POOL_SIZE },
                    { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          DESCRIPTOR_POOL_SIZE },
                    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         DESCRIPTOR_POOL_SIZE },
                    { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          DESCRIPTOR_POOL_SIZE },
                    { VK_DESCRIPTOR_TYPE_SAMPLER,                DESCRIPTOR_POOL_SIZE },
                };

            VkDescriptorPoolCreateInfo poolInfo = {};
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolInfo.flags = 0;
            poolInfo.maxSets = uint32_t(sizes.size()) * DESCRIPTOR_POOL_SIZE;
            poolInfo.poolSizeCount = uint32_t(sizes.size());
            poolInfo.pPoolSizes = sizes.data();

            VkDescriptorPool pool;
            VK_CHECK(vkCreateDescriptorPool(device->device, &poolInfo, nullptr, &pool))

            return pool;

        }

    }

}