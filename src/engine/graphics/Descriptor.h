#ifndef AE_GRAPHICSDESCRIPTOR_H
#define AE_GRAPHICSDESCRIPTOR_H

#include "Common.h"

#include <vector>

namespace Atlas {

    namespace Graphics {

        class GraphicsDevice;
        class MemoryManager;

        class DescriptorPool {

        public:
            explicit DescriptorPool(GraphicsDevice* device);

            ~DescriptorPool();

            void Reset();

            VkDescriptorSet Allocate(VkDescriptorSetLayout layout);

            VkDescriptorPool GetNativePool();

        private:
            VkDescriptorPool InitPool();

            GraphicsDevice* device;
            std::vector<VkDescriptorPool> pools;

            uint32_t poolIdx = 0;

        };

    }

}

#endif