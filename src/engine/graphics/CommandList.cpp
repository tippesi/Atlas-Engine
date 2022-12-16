#include "CommandList.h"

#include <cassert>

namespace Atlas {

    namespace Graphics {

        CommandList::CommandList(MemoryManager* memManager, QueueType queueType, uint32_t queueFamilyIndex) :
            device(memManager->device), queueType(queueType), queueFamilyIndex(queueFamilyIndex) {

            VkCommandPoolCreateInfo poolCreateInfo = Initializers::InitCommandPoolCreateInfo(queueFamilyIndex);
            VK_CHECK(vkCreateCommandPool(device, &poolCreateInfo, nullptr, &commandPool))

            VkCommandBufferAllocateInfo bufferAllocateInfo = Initializers::InitCommandBufferAllocateInfo(commandPool, 1);
            VK_CHECK(vkAllocateCommandBuffers(device, &bufferAllocateInfo, &commandBuffer))

            VkSemaphoreCreateInfo semaphoreInfo = Initializers::InitSemaphoreCreateInfo();
            VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore))

            descriptorPool = new DescriptorPool(memManager);

            isComplete = true;

        }

        CommandList::~CommandList() {

            delete descriptorPool;

            vkDestroySemaphore(device, semaphore, nullptr);
            vkDestroyCommandPool(device, commandPool, nullptr);

        }

        void CommandList::BeginCommands() {

            // Here we assume the command buffer is free to write and not used
            // in an ongoing GPU operation
            VK_CHECK(vkResetCommandPool(device, commandPool, 0))

            VkCommandBufferBeginInfo cmdBeginInfo = {};
            cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            cmdBeginInfo.pNext = nullptr;
            cmdBeginInfo.pInheritanceInfo = nullptr;
            cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

            VK_CHECK(vkBeginCommandBuffer(commandBuffer, &cmdBeginInfo))

        }

        void CommandList::EndCommands() {

            VK_CHECK(vkEndCommandBuffer(commandBuffer));

        }

        void CommandList::BeginRenderPass(SwapChain* swapChain) {

            swapChain->AquireImageIndex();

            VkRenderPassBeginInfo rpInfo = {};
            rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpInfo.pNext = nullptr;

            rpInfo.renderPass = swapChain->defaultRenderPass;
            rpInfo.renderArea.offset.x = 0;
            rpInfo.renderArea.offset.y = 0;
            rpInfo.renderArea.extent = swapChain->extent;
            rpInfo.framebuffer = swapChain->frameBuffers[swapChain->aquiredImageIndex];

            VkClearValue clearValues[] = { swapChain->colorClearValue, swapChain->depthClearValue };
            rpInfo.clearValueCount = 2;
            rpInfo.pClearValues = clearValues;

            vkCmdBeginRenderPass(commandBuffer, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

            swapChainInUse = swapChain;

        }

        void CommandList::BeginRenderPass(RenderPass *renderPass) {

            renderPassInUse = renderPass;

        }

        void CommandList::EndRenderPass() {

            vkCmdEndRenderPass(commandBuffer);

            swapChainInUse = nullptr;
            renderPassInUse = nullptr;

        }

        void CommandList::BindPipeline(Pipeline *pipeline) {

            pipelineInUse = pipeline;

            vkCmdBindPipeline(commandBuffer, pipeline->bindPoint, pipeline->pipeline);

            // Set default values for viewport/scissor, they are required
            auto extent = GetRenderPassExtent();
            SetViewport(0, 0, extent.width, extent.height);
            SetScissor(0, 0, extent.width, extent.height);

        }

        void CommandList::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {

            assert(pipelineInUse && "No pipeline is bound");
            if (!pipelineInUse) return;

            VkViewport viewport = {};
            viewport.x = float(x);
            viewport.y = float(y);
            viewport.width = float(width);
            viewport.height = float(height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        }

        void CommandList::SetScissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {

            assert(pipelineInUse && "No pipeline is bound");
            if (!pipelineInUse) return;

            VkRect2D scissor = {};
            scissor.offset.x = x;
            scissor.offset.y = y;
            scissor.extent.width = width;
            scissor.extent.height = height;

            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        }

        void CommandList::PushConstants(PushConstantRange *pushConstantRange, void *data) {

            assert(pipelineInUse && "No pipeline is bound");
            if (!pipelineInUse) return;

            vkCmdPushConstants(commandBuffer, pipelineInUse->layout, pushConstantRange->range.stageFlags,
                pushConstantRange->range.offset, pushConstantRange->range.size, data);

        }

        void CommandList::BindIndexBuffer(IndexBuffer *buffer) {

            assert(pipelineInUse && "No pipeline is bound");
            if (!pipelineInUse || !buffer->buffer) return;

            vkCmdBindIndexBuffer(commandBuffer, buffer->buffer->buffer, 0, buffer->type);

        }

        void CommandList::BindVertexBuffer(VertexBuffer *buffer) {

            assert(pipelineInUse && "No pipeline is bound");
            if (!pipelineInUse || !buffer->buffer) return;

            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(commandBuffer, buffer->bindingDescription.binding, 1,
                &buffer->buffer->buffer, &offset);

        }

        void CommandList::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex,
            int32_t vertexOffset, uint32_t firstInstance) {

            assert(pipelineInUse && "No pipeline is bound");
            if (!pipelineInUse) return;

            assert(indexCount && instanceCount && "Index or instance count should not be zero");
            vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);

        }

        const VkExtent2D CommandList::GetRenderPassExtent() const {

            VkExtent2D extent = {};

            if (swapChainInUse) {
                extent = swapChainInUse->extent;
            }
            else if (renderPassInUse) {
                // TODO...
            }
            else {
                assert(0 && "No valid render pass found");
            }

            return extent;

        }

    }

}