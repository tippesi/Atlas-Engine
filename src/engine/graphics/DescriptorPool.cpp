#include "DescriptorPool.h"
#include "GraphicsDevice.h"

namespace Atlas {

    namespace Graphics {

        DescriptorPool::DescriptorPool(GraphicsDevice* device) : device(device) {

            DescriptorSetSize size = {};
            pools.push_back(InitPool(size));

        }

        DescriptorPool::~DescriptorPool() {

            for (auto pool : pools) {
                vkDestroyDescriptorPool(device->device, pool, nullptr);
            }

        }

        Ref<DescriptorSet> DescriptorPool::GetCachedSet(const Ref<DescriptorSetLayout>& layout) {

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

        Ref<DescriptorSet> DescriptorPool::Allocate(const Ref<DescriptorSetLayout>& layout) {

            VkDescriptorSetAllocateInfo allocInfo = {};
            allocInfo.pNext = nullptr;
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = pools[poolIdx];
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &layout->layout;

            /*
            VkDescriptorSetVariableDescriptorCountAllocateInfo countInfo = {};
            std::vector<uint32_t> bindlessDescriptorCount;
            if (layout->bindless) {
                for (auto& binding : layout->bindings) {
                    if (!binding.bindless) continue;
                    bindlessDescriptorCount.push_back(binding.descriptorCount);
                }

                countInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
                countInfo.descriptorSetCount = uint32_t(bindlessDescriptorCount.size());
                countInfo.pDescriptorCounts = bindlessDescriptorCount.data();
                allocInfo.pNext = &countInfo;
            }
            */

            Ref<DescriptorSet> set = CreateRef<DescriptorSet>();
            auto result = vkAllocateDescriptorSets(device->device, &allocInfo, &set->set);
            // Handle the pool out of memory error by allocating a new pool
            if (result == VK_ERROR_OUT_OF_POOL_MEMORY) {
                poolIdx++;
                if (poolIdx == pools.size()) {
                    pools.push_back(InitPool(layout->size));
                }
                allocInfo.descriptorPool = pools[poolIdx];
                VK_CHECK(vkAllocateDescriptorSets(device->device, &allocInfo, &set->set))
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

        VkDescriptorPool DescriptorPool::InitPool(const DescriptorSetSize& size) {

            std::vector<VkDescriptorPoolSize> sizes = {
                    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, std::max(DESCRIPTOR_POOL_SIZE, size.dynamicUniformBufferCount) },
                    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         std::max(DESCRIPTOR_POOL_SIZE, size.uniformBufferCount) },
                    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, std::max(DESCRIPTOR_POOL_SIZE, size.combinedImageSamplerCount) },
                    { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          std::max(DESCRIPTOR_POOL_SIZE, size.sampledImageCount) },
                    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, std::max(DESCRIPTOR_POOL_SIZE, size.dynamicStorageBufferCount) },
                    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         std::max(DESCRIPTOR_POOL_SIZE, size.storageBufferCount) },
                    { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          std::max(DESCRIPTOR_POOL_SIZE, size.storageImageCount) },
                    { VK_DESCRIPTOR_TYPE_SAMPLER,                std::max(DESCRIPTOR_POOL_SIZE, size.samplerCount) },
                };

            VkDescriptorPoolCreateInfo poolInfo = {};
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
            poolInfo.maxSets = uint32_t(sizes.size()) * DESCRIPTOR_POOL_SIZE;
            poolInfo.poolSizeCount = uint32_t(sizes.size());
            poolInfo.pPoolSizes = sizes.data();

            VkDescriptorPool pool;
            VK_CHECK(vkCreateDescriptorPool(device->device, &poolInfo, nullptr, &pool))

            return pool;

        }

    }

}