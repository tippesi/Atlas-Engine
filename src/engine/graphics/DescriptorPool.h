#pragma once

#include "Common.h"
#include "DescriptorSetLayout.h"

#include <vector>
#include <unordered_map>

namespace Atlas {

    namespace Graphics {

        class GraphicsDevice;
        class MemoryManager;

        struct DescriptorSet {
            VkDescriptorSet set;

            uint32_t sampledImageArraySize[BINDINGS_PER_DESCRIPTOR_SET] = {};
            uint32_t bufferArraySize[BINDINGS_PER_DESCRIPTOR_SET] = {};
        };

        class DescriptorPool {

        public:
            explicit DescriptorPool(GraphicsDevice* device);

            ~DescriptorPool();

            void Reset();

            void ResetAllocationCounters();

            Ref<DescriptorSet> GetCachedSet(const Ref<DescriptorSetLayout>& layout);

            Ref<DescriptorSet> Allocate(const Ref<DescriptorSetLayout>& layout);

            VkDescriptorPool GetNativePool();

        private:
            struct LayoutAllocations {
                std::vector<Ref<DescriptorSet>> sets;
                size_t counter = 0;
            };

            VkDescriptorPool InitPool(const DescriptorSetSize& size);

            GraphicsDevice* device;
            std::vector<VkDescriptorPool> pools;
            std::unordered_map<Ref<DescriptorSetLayout>, LayoutAllocations> layoutAllocationsMap;

            uint32_t poolIdx = 0;

        };

    }

}