#ifndef AE_GRAPHICSCOMMANDLIST_H
#define AE_GRAPHICSCOMMANDLIST_H

#include "Common.h"
#include "Barrier.h"
#include "MemoryManager.h"
#include "SwapChain.h"
#include "RenderPass.h"
#include "Pipeline.h"
#include "Buffer.h"
#include "Descriptor.h"
#include "Sampler.h"
#include "QueryPool.h"

#include "../common/Ref.h"

#include <atomic>
#include <vector>

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
            CommandList(GraphicsDevice* device, QueueType queueType, uint32_t queueFamilyIndex,
                bool frameIndependent = false);

            CommandList(const CommandList& that) = delete;

            ~CommandList();

            CommandList& operator=(const CommandList& that) = delete;

            void BeginCommands();

            void EndCommands();

            void BeginRenderPass(SwapChain* swapChain, bool clear = false);

            void BeginRenderPass(const Ref<RenderPass>& renderPass, const Ref<FrameBuffer>& frameBuffer,
                bool clear = false, bool autoAdjustImageLayouts = false);

            void EndRenderPass();

            void BeginQuery(const Ref<QueryPool>& queryPool, uint32_t queryIdx);

            void EndQuery(const Ref<QueryPool>& queryPool, uint32_t queryIdx);

            void Timestamp(const Ref<QueryPool>& queryPool, uint32_t queryIdx);

            void BindPipeline(const Ref<Pipeline>& pipeline);

            void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

            void SetScissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

            void ClearAttachments();

            void PushConstants(PushConstantRange* pushConstantRange, void* data);

            void BindIndexBuffer(IndexBuffer* buffer);

            void BindVertexBuffer(VertexBuffer* buffer);

            void BindBuffer(const Ref<Buffer>& buffer, uint32_t set, uint32_t binding);

            void BindBuffer(const Ref<MultiBuffer>& buffer, uint32_t set, uint32_t binding);

            void BindImage(const Ref<Image>& image, uint32_t set, uint32_t binding);

            void BindImage(const Ref<Image>& image, Ref<Sampler>& sampler, uint32_t set, uint32_t binding);

            void ResetBindings();

            void ImageMemoryBarrier(ImageBarrier& barrier, VkPipelineStageFlags srcStageMask,
                VkPipelineStageFlags dstStageMask);

            void BufferMemoryBarrier(BufferBarrier& barrier, VkPipelineStageFlags srcStageMask,
                VkPipelineStageFlags dstStageMask);

            void PipelineBarrier(std::vector<ImageBarrier>& imageBarriers, std::vector<BufferBarrier>& bufferBarriers,
                VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask);

            void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0,
                int32_t vertexOffset = 0, uint32_t firstInstance = 0);

            void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0,
                uint32_t firstInstance = 0);

            void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

            void CopyBuffer(const Ref<Buffer>& srcBuffer, const Ref<Buffer>& dstBuffer);

            void CopyBuffer(const Ref<Buffer>& srcBuffer, const Ref<Buffer>& dstBuffer, VkBufferCopy copy);

            void CopyImage(const Ref<Image>& srcImage, const Ref<Image>& dstImage);

            void CopyImage(const Ref<Image>& srcImage, const Ref<Image>& dstImage, VkImageCopy copy);

            void GenerateMipMap(const Ref<Image>& image);

            VkCommandPool commandPool;
            VkCommandBuffer commandBuffer;
            VkSemaphore semaphore;
            VkFence fence;
            uint32_t queueFamilyIndex;

            bool isComplete = false;
            const bool frameIndependent = false;

            QueueType queueType;

            SwapChain* swapChainInUse = nullptr;
            Ref<RenderPass> renderPassInUse = nullptr;
            Ref<Pipeline> pipelineInUse = nullptr;
            Ref<FrameBuffer> frameBufferInUse = nullptr;

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
                        sets[i] = nullptr;
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
            MemoryManager* memoryManager = nullptr;
            DescriptorPool* descriptorPool = nullptr;

            std::atomic_bool isLocked = true;
            std::atomic_bool isSubmitted = true;

        };

    }

}

#endif