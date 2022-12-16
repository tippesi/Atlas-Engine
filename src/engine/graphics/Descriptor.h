#ifndef AE_DESCRIPTOR_H
#define AE_DESCRIPTOR_H

#include "Common.h"
#include "MemoryManager.h"

#include <vector>

namespace Atlas {

    namespace Graphics {

        struct DescriptorAllocationDesc {
            // VkDescriptorBufferInfo
        };

        struct DescriptorAllocation {
            VkDescriptorSet uniformBufferSets[BINDINGS_PER_DESCRIPTOR_SET];
        };

        class DescriptorPool {

        public:
            DescriptorPool(MemoryManager* memManager);

            ~DescriptorPool();

            void Reset();

            DescriptorAllocation Allocate(const DescriptorAllocationDesc& desc);

        private:
            VkDescriptorPool InitPool();

            MemoryManager* memoryManager;
            std::vector<VkDescriptorPool> pools;

        };

        class DescriptorBindings {

        };

    }

}

#endif