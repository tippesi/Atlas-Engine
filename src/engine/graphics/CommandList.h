#ifndef AE_GRAPHICSCOMMANDLIST_H
#define AE_GRAPHICSCOMMANDLIST_H

#include "Common.h"
#include "SwapChain.h"

namespace Atlas {

    namespace Graphics {

        enum QueueType {
            GraphicsQueue = 0,
            PresentationQueue
        };

        class CommandList {
        public:
            CommandList(VkDevice device, QueueType queueType, uint32_t queueFamilyIndex);

            ~CommandList();

            void BeginCommands();

            void EndCommands();

            void BeginRenderPass(SwapChain* swapChain);

            void EndRenderPass();

            VkCommandPool commandPool;
            VkCommandBuffer commandBuffer;
            VkSemaphore semaphore;
            uint32_t queueFamilyIndex;

            bool isComplete = false;

            QueueType queueType;

        private:
            VkDevice device;

            SwapChain* currentSwapChain;

        };

    }

}

#endif