#ifndef AE_GRAPHICSCOMMANDLIST_H
#define AE_GRAPHICSCOMMANDLIST_H

#include "Common.h"
#include "MemoryManager.h"
#include "SwapChain.h"
#include "RenderPass.h"
#include "Pipeline.h"
#include "Buffer.h"
#include "Descriptor.h"

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
            CommandList(MemoryManager* memManager, QueueType queueType, uint32_t queueFamilyIndex);

            ~CommandList();

            void BeginCommands();

            void EndCommands();

            void BeginRenderPass(SwapChain* swapChain);

            void BeginRenderPass(RenderPass* renderPass);

            void EndRenderPass();

            void BindPipeline(Pipeline* pipeline);

            void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

            void SetScissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

            void PushConstants(PushConstantRange* pushConstantRange, void* data);

            void BindIndexBuffer(IndexBuffer* buffer);

            void BindVertexBuffer(VertexBuffer* buffer);

            void BindBuffer(Buffer* buffer, uint32_t set, uint32_t binding);

            void BindImage(Image* buffer, uint32_t set, uint32_t binding);

            void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0,
                int32_t vertexOffset = 0, uint32_t firstInstance = 0);

            VkCommandPool commandPool;
            VkCommandBuffer commandBuffer;
            VkSemaphore semaphore;
            uint32_t queueFamilyIndex;

            bool isComplete = false;

            QueueType queueType;

            SwapChain* swapChainInUse = nullptr;
            RenderPass* renderPassInUse = nullptr;
            Pipeline* pipelineInUse = nullptr;

        private:
            const VkExtent2D GetRenderPassExtent() const;

            VkDevice device;
            DescriptorPool* descriptorPool = nullptr;

            std::atomic_bool isLocked = false;

        };

    }

}

#endif