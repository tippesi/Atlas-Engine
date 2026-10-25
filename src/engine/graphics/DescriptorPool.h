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
            struct WrittenImage {
                uint64_t id = 0;
                VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
            };

            VkDescriptorSet set;

            std::vector<WrittenImage> sampledImageArrays[BINDINGS_PER_DESCRIPTOR_SET];
            std::vector<uint64_t> bufferArrays[BINDINGS_PER_DESCRIPTOR_SET];
        };

        struct DescriptorPoolDesc {
            bool freeDescriptorSets = false;
        };

        class DescriptorPool {

        public:
            explicit DescriptorPool(GraphicsDevice* device, const DescriptorPoolDesc& desc);

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

            bool PoolFits(const DescriptorSetSize& poolSize, const DescriptorSetSize& requestedSize) const;

            GraphicsDevice* device;
            std::vector<VkDescriptorPool> pools;
            std::vector<DescriptorSetSize> poolSizes;
            std::unordered_map<Ref<DescriptorSetLayout>, LayoutAllocations> layoutAllocationsMap;

            uint32_t poolIdx = 0;

            bool freeDescriptorSets = false;

        };

    }

}
