#pragma once

#include "Common.h"
#include "DescriptorSet.h"

#include <vector>
#include <unordered_map>

namespace Atlas {

    namespace Graphics {

        class GraphicsDevice;
        class MemoryManager;

        class DescriptorPool {

        public:
            explicit DescriptorPool(GraphicsDevice* device);

            ~DescriptorPool();

            void Reset();

            void ResetAllocationCounters();

            VkDescriptorSet GetCachedSet(Ref<DescriptorSetLayout>& layout);

            VkDescriptorSet Allocate(Ref<DescriptorSetLayout>& layout);

            VkDescriptorPool GetNativePool();

        private:
            struct LayoutAllocations {
                std::vector<VkDescriptorSet> sets;
                size_t counter = 0;
            };

            VkDescriptorPool InitPool(DescriptorSetSize& size);

            GraphicsDevice* device;
            std::vector<VkDescriptorPool> pools;
            std::unordered_map<Ref<DescriptorSetLayout>, LayoutAllocations> layoutAllocationsMap;

            uint32_t poolIdx = 0;

        };

    }

}