#ifndef AE_GRAPHICSCOMMANDLIST_H
#define AE_GRAPHICSCOMMANDLIST_H

#include "Common.h"
#include "MemoryManager.h"
#include "SwapChain.h"
#include "RenderPass.h"
#include "Pipeline.h"
#include "Buffer.h"
#include "Descriptor.h"
#include "Sampler.h"

#include <atomic>

namespace Atlas {

    namespace Graphics {

        enum QueueType {
            GraphicsQueue = 0,
            PresentationQueue,
            TransferQueue
        };

        class GraphicsDevice;
        class FrameData;

        class CommandList {

            friend GraphicsDevice;
            friend FrameData;

        public:
            CommandList(GraphicsDevice* device, QueueType queueType, uint32_t queueFamilyIndex);

            ~CommandList();

            void BeginCommands();

            void EndCommands();

            void BeginRenderPass(SwapChain* swapChain, bool clear = false);

            void BeginRenderPass(RenderPass* renderPass, bool clear = false);

            void EndRenderPass();

            void BindPipeline(Pipeline* pipeline);

            void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

            void SetScissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

            void ClearAttachments();

            void PushConstants(PushConstantRange* pushConstantRange, void* data);

            void BindIndexBuffer(IndexBuffer* buffer);

            void BindVertexBuffer(VertexBuffer* buffer);

            void BindBuffer(Buffer* buffer, uint32_t set, uint32_t binding);

            void BindImage(Image* image, uint32_t set, uint32_t binding);

            void BindImage(Image* image, Sampler* sampler, uint32_t set, uint32_t binding);

            void ResetBindings();

            // void ImageBarrier(Image* image, )

            void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0,
                int32_t vertexOffset = 0, uint32_t firstInstance = 0);

            VkCommandPool commandPool;
            VkCommandBuffer commandBuffer;
            VkSemaphore semaphore;
            VkFence fence;
            uint32_t queueFamilyIndex;

            bool isComplete = false;

            QueueType queueType;

            SwapChain* swapChainInUse = nullptr;
            RenderPass* renderPassInUse = nullptr;
            Pipeline* pipelineInUse = nullptr;

        private:
            struct DescriptorBindingData {
                Buffer* buffers[DESCRIPTOR_SET_COUNT][BINDINGS_PER_DESCRIPTOR_SET];
                Image* images[DESCRIPTOR_SET_COUNT][BINDINGS_PER_DESCRIPTOR_SET];
                std::pair<Image*, Sampler*> sampledImages[DESCRIPTOR_SET_COUNT][BINDINGS_PER_DESCRIPTOR_SET];

                VkDescriptorSet sets[DESCRIPTOR_SET_COUNT];

                DescriptorBindingData() {
                    Reset();
                    for (uint32_t i = 0; i < DESCRIPTOR_SET_COUNT; i++) {
                        sets[i] = nullptr;
                    }
                }

                void Reset() {
                    for (uint32_t i = 0; i < DESCRIPTOR_SET_COUNT; i++) {
                        for (uint32_t j = 0; j <  BINDINGS_PER_DESCRIPTOR_SET; j++) {
                            buffers[i][j] = nullptr;
                            images[i][j] = nullptr;
                            sampledImages[i][j] = { nullptr, nullptr };
                        }
                    }
                }

                bool IsEqual(const DescriptorBindingData& that, uint32_t set) {
                    for (uint32_t i = 0; i < BINDINGS_PER_DESCRIPTOR_SET; i++) {
                        if (buffers[set][i] != that.buffers[set][i] ||
                            images[set][i] != that.images[set][i] ||
                            sampledImages[set][i] != that.sampledImages[set][i])
                            return false;
                    }
                    return true;
                }
            }descriptorBindingData, prevDescriptorBindingData;

            void BindDescriptorSets();

            void ResetDescriptors();

            const VkExtent2D GetRenderPassExtent() const;

            VkDevice device;
            DescriptorPool* descriptorPool = nullptr;

            std::atomic_bool isLocked = true;

        };

    }

}

#endif