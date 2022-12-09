#ifndef AE_GRAPHICSCOMMANDLIST_H
#define AE_GRAPHICSCOMMANDLIST_H

#include "Common.h"

namespace Atlas {

    namespace Graphics {

        class CommandList {
        public:
            CommandList(VkDevice device, uint32_t queueFamilyIndex);

            ~CommandList();

            VkCommandPool commandPool;
            VkCommandBuffer commandBuffer;

            bool isComplete = false;

        private:
            VkDevice device;

        };

    }

}

#endif