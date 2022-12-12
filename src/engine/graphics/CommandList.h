#ifndef AE_GRAPHICSCOMMANDLIST_H
#define AE_GRAPHICSCOMMANDLIST_H

#include "Common.h"
#include "SwapChain.h"
#include "Pipeline.h"

#include <atomic>

namespace Atlas {

    namespace Graphics {

        enum QueueType {
            GraphicsQueue = 0,
            PresentationQueue,
            TransferQueue
        };

        class GraphicsDevice;

        class CommandList {

            friend GraphicsDevice;

        public:
            CommandList(VkDevice device, QueueType queueType, uint32_t queueFamilyIndex);

            ~CommandList();

            void BeginCommands();

            void EndCommands();

            void BeginRenderPass(SwapChain* swapChain);

            void EndRenderPass();

            void BindPipeline(Pipeline* pipeline);

            void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

            VkCommandPool commandPool;
            VkCommandBuffer commandBuffer;
            VkSemaphore semaphore;
            uint32_t queueFamilyIndex;

            bool isComplete = false;

            QueueType queueType;

            SwapChain* swapChainInUse = nullptr;
            Pipeline* pipelineInUse = nullptr;

        private:
            VkDevice device;
            std::atomic_bool isLocked = false;

            SwapChain* currentSwapChain;

        };

    }

}

#endif