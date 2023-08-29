#ifndef AE_GRAPHICSCOMMANDLIST_H
#define AE_GRAPHICSCOMMANDLIST_H

#include "Common.h"
#include "Queue.h"
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

        enum class ExecutionOrder {
            Sequential = 0,
            Parallel = 1
        };

        class GraphicsDevice;
        class FrameData;

        class CommandList {

            friend GraphicsDevice;
            friend FrameData;

        public:
            CommandList(GraphicsDevice* device, QueueType queueType, uint32_t queueFamilyIndex,
                const std::vector<VkQueue>& queues, bool frameIndependent = false);

            CommandList(const CommandList& that) = delete;

            ~CommandList();

            CommandList& operator=(const CommandList& that) = delete;

            void DependsOn(CommandList* commandList);

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

            void PushConstants(const std::string& pushConstantRangeName, void* data);

            void BindIndexBuffer(const Ref<Buffer>& buffer, VkIndexType type);

            void BindVertexBuffer(const Ref<Buffer>& buffer, uint32_t binding);

            void BindBuffer(const Ref<Buffer>& buffer, uint32_t set, uint32_t binding);

            void BindBufferOffset(const Ref<Buffer>& buffer, size_t offset, uint32_t set, uint32_t binding);

            void BindBuffer(const Ref<MultiBuffer>& buffer, uint32_t set, uint32_t binding);

            void BindBufferOffset(const Ref<MultiBuffer>& buffer, size_t offset, uint32_t set, uint32_t binding);

            void BindImage(const Ref<Image>& image, uint32_t set, uint32_t binding, uint32_t mipLevel = 0);

            void BindImage(const Ref<Image>& image, const Ref<Sampler>& sampler, uint32_t set, uint32_t binding);

            void BindTLAS(const Ref<TLAS>& tlas, uint32_t set, uint32_t binding);

            void ResetBindings();

            void ImageMemoryBarrier(const Ref<Image>& image, VkImageLayout newLayout, VkAccessFlags newAccessMask,
                VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            void ImageMemoryBarrier(ImageBarrier& barrier,
                VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            void ImageTransition(const Ref<Image>& image, VkImageLayout newLayout, VkAccessFlags newAccessMask);

            void BufferMemoryBarrier(const Ref<Buffer>& buffer, VkAccessFlags newAccessMask,
                VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            void BufferMemoryBarrier(const Ref<MultiBuffer>& buffer, VkAccessFlags newAccessMask,
                VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            void BufferMemoryBarrier(BufferBarrier& barrier, VkPipelineStageFlags srcStageMask,
                VkPipelineStageFlags dstStageMask);

            void MemoryBarrier(VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
                VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            void PipelineBarrier(VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            void PipelineBarrier(std::vector<ImageBarrier>& imageBarriers, std::vector<BufferBarrier>& bufferBarriers,
                VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0,
                int32_t vertexOffset = 0, uint32_t firstInstance = 0);

            void DrawIndexedIndirect(const Ref<Buffer>& buffer, size_t offset = 0,
                uint32_t drawCount = 1, uint32_t stride = 0);

            void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0,
                uint32_t firstInstance = 0);

            void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

            void DispatchIndirect(const Ref<Buffer>& buffer, uint32_t offset = 0);

            void CopyBuffer(const Ref<Buffer>& srcBuffer, const Ref<Buffer>& dstBuffer);

            void CopyBuffer(const Ref<Buffer>& srcBuffer, const Ref<Buffer>& dstBuffer, VkBufferCopy copy);

            void FillBuffer(const Ref<Buffer>& buffer, void* data);

            void FillBuffer(const Ref<MultiBuffer>& buffer, void* data);

            void CopyImage(const Ref<Image>& srcImage, const Ref<Image>& dstImage);

            void CopyImage(const Ref<Image>& srcImage, const Ref<Image>& dstImage, VkImageCopy copy);

            void BlitImage(const Ref<Image>& srcImage, const Ref<Image>& dstImage);

            void BlitImage(const Ref<Image>& srcImage, const Ref<Image>& dstImage, VkImageBlit blit);

            void GenerateMipMap(const Ref<Image>& image);

            void BuildBLAS(const Ref<BLAS>& blas, VkAccelerationStructureBuildGeometryInfoKHR& buildInfo);

            void BuildTLAS(const Ref<TLAS>& tlas, VkAccelerationStructureBuildGeometryInfoKHR& buildInfo);

            VkCommandPool commandPool;
            VkCommandBuffer commandBuffer;
            VkFence fence;
            uint32_t queueFamilyIndex;

            bool isComplete = false;
            const bool frameIndependent = false;

            QueueType queueType;

            SwapChain* swapChainInUse = nullptr;
            Ref<RenderPass> renderPassInUse = nullptr;
            Ref<Pipeline> pipelineInUse = nullptr;
            Ref<FrameBuffer> frameBufferInUse = nullptr;

            std::vector<CommandList*> dependencies;
            ExecutionOrder executionOrder = ExecutionOrder::Sequential;

            int32_t id = 0;

        private:
            struct DescriptorBindingData {
                Buffer* buffers[DESCRIPTOR_SET_COUNT][BINDINGS_PER_DESCRIPTOR_SET];
                std::pair<Buffer*, uint32_t> dynamicBuffers[DESCRIPTOR_SET_COUNT][BINDINGS_PER_DESCRIPTOR_SET];
                std::pair<Image*, uint32_t> images[DESCRIPTOR_SET_COUNT][BINDINGS_PER_DESCRIPTOR_SET];
                std::pair<Image*, Sampler*> sampledImages[DESCRIPTOR_SET_COUNT][BINDINGS_PER_DESCRIPTOR_SET];
                TLAS* tlases[DESCRIPTOR_SET_COUNT][BINDINGS_PER_DESCRIPTOR_SET];

                VkDescriptorSet sets[DESCRIPTOR_SET_COUNT];
                bool changed[DESCRIPTOR_SET_COUNT];

                DescriptorBindingData() {
                    Reset();
                    for (uint32_t i = 0; i < DESCRIPTOR_SET_COUNT; i++) {
                        sets[i] = nullptr;
                        changed[i] = true;
                    }
                }

                void Reset() {
                    for (uint32_t i = 0; i < DESCRIPTOR_SET_COUNT; i++) {
                        for (uint32_t j = 0; j <  BINDINGS_PER_DESCRIPTOR_SET; j++) {
                            buffers[i][j] = nullptr;
                            dynamicBuffers[i][j] = { nullptr, 0u };
                            images[i][j] = { nullptr, 0u };
                            sampledImages[i][j] = { nullptr, nullptr };
                            tlases[i][j] = nullptr;
                        }
                        sets[i] = nullptr;
                        changed[i] = true;
                    }
                }

                void Reset(uint32_t set) {
                    for (uint32_t j = 0; j <  BINDINGS_PER_DESCRIPTOR_SET; j++) {
                        buffers[set][j] = nullptr;
                        dynamicBuffers[set][j] = { nullptr, 0u };
                        images[set][j] = { nullptr, 0u };
                        sampledImages[set][j] = { nullptr, nullptr };
                        tlases[set][j] = nullptr;
                    }
                    sets[set] = nullptr;
                    changed[set] = true;
                }
            }descriptorBindingData;

            struct Semaphore {
                VkSemaphore semaphore;
                VkQueue queue;
            };

            void BindDescriptorSets();

            void ResetDescriptors();

            const VkExtent2D GetRenderPassExtent() const;

            const VkSemaphore GetSemaphore(VkQueue queue);

            const std::vector<VkSemaphore> GetSemaphores() const;

            VkDevice device;
            MemoryManager* memoryManager = nullptr;
            DescriptorPool* descriptorPool = nullptr;

            std::atomic_bool isLocked = true;
            std::atomic_bool isSubmitted = true;
            std::atomic_bool wasSwapChainAccessed = false;

            std::vector<Semaphore> semaphores;

        };

    }

}

#endif