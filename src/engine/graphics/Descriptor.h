#ifndef AE_GRAPHICSDESCRIPTOR_H
#define AE_GRAPHICSDESCRIPTOR_H

#include "Common.h"

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

            VkDescriptorSet GetCachedSet(VkDescriptorSetLayout layout);

            VkDescriptorSet Allocate(VkDescriptorSetLayout layout);

            VkDescriptorPool GetNativePool();

        private:
            struct LayoutAllocations {
                std::vector<VkDescriptorSet> sets;
                size_t counter = 0;
            };

            VkDescriptorPool InitPool();

            GraphicsDevice* device;
            std::vector<VkDescriptorPool> pools;
            std::unordered_map<VkDescriptorSetLayout, LayoutAllocations> layoutAllocationsMap;

            uint32_t poolIdx = 0;

        };

    }

}

#endif