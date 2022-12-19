#ifndef AE_GRAPHICSDESCRIPTOR_H
#define AE_GRAPHICSDESCRIPTOR_H

#include "Common.h"
#include "MemoryManager.h"

#include <vector>

namespace Atlas {

    namespace Graphics {

        class GraphicsDevice;

        class DescriptorPool {

        public:
            DescriptorPool(GraphicsDevice* device);

            ~DescriptorPool();

            void Reset();

            VkDescriptorSet Allocate(VkDescriptorSetLayout layout);

            VkDescriptorPool GetNativePool();

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