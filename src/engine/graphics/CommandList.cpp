#include "CommandList.h"

namespace Atlas {

    namespace Graphics {

        CommandList::CommandList(VkDevice device, QueueType queueType, uint32_t queueFamilyIndex) : device(device),
            queueType(queueType), queueFamilyIndex(queueFamilyIndex) {

            VkCommandPoolCreateInfo poolCreateInfo = Initializers::InitCommandPoolCreateInfo(queueFamilyIndex);
            VK_CHECK(vkCreateCommandPool(device, &poolCreateInfo, nullptr, &commandPool))

            VkCommandBufferAllocateInfo bufferAllocateInfo = Initializers::InitCommandBufferAllocateInfo(commandPool, 1);
            VK_CHECK(vkAllocateCommandBuffers(device, &bufferAllocateInfo, &commandBuffer))

            VkSemaphoreCreateInfo semaphoreInfo = Initializers::InitSemaphoreCreateInfo();
            VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore))

            isComplete = true;

        }

        CommandList::~CommandList() {

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

        }

        void CommandList::EndRenderPass() {

            vkCmdEndRenderPass(commandBuffer);

        }

        void CommandList::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {

            // if (!pipelineInUse) return;

            VkViewport viewport = {};
            viewport.x = float(x);
            viewport.y = float(y);
            viewport.width = float(width);
            viewport.height = float(height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        }

    }

}