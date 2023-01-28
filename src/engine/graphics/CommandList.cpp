#include "CommandList.h"
#include "GraphicsDevice.h"

#include <cassert>

namespace Atlas {

    namespace Graphics {

        CommandList::CommandList(GraphicsDevice* device, QueueType queueType, uint32_t queueFamilyIndex,
            bool frameIndependent) : memoryManager(device->memoryManager), device(device->device),
            frameIndependent(frameIndependent), queueType(queueType), queueFamilyIndex(queueFamilyIndex) {

            VkCommandPoolCreateInfo poolCreateInfo = Initializers::InitCommandPoolCreateInfo(queueFamilyIndex);
            VK_CHECK(vkCreateCommandPool(device->device, &poolCreateInfo, nullptr, &commandPool))

            VkCommandBufferAllocateInfo bufferAllocateInfo = Initializers::InitCommandBufferAllocateInfo(commandPool, 1);
            VK_CHECK(vkAllocateCommandBuffers(device->device, &bufferAllocateInfo, &commandBuffer))

            VkSemaphoreCreateInfo semaphoreInfo = Initializers::InitSemaphoreCreateInfo();
            VK_CHECK(vkCreateSemaphore(device->device, &semaphoreInfo, nullptr, &semaphore))

            VkFenceCreateInfo fenceInfo = Initializers::InitFenceCreateInfo();
            VK_CHECK(vkCreateFence(device->device, &fenceInfo, nullptr, &fence))

            descriptorPool = new DescriptorPool(device);

            isComplete = true;

        }

        CommandList::~CommandList() {

            delete descriptorPool;

            vkDestroySemaphore(device, semaphore, nullptr);
            vkDestroyFence(device, fence, nullptr);
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

        void CommandList::BeginRenderPass(SwapChain* swapChain, bool clear) {

            auto imageIdx = swapChain->aquiredImageIndex;

            VkRenderPassBeginInfo rpInfo = {};
            rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpInfo.pNext = nullptr;
            rpInfo.renderPass = swapChain->renderPass;
            rpInfo.renderArea.offset.x = 0;
            rpInfo.renderArea.offset.y = 0;
            rpInfo.renderArea.extent = swapChain->extent;
            rpInfo.framebuffer = swapChain->frameBuffers[imageIdx];

            // Transition images to something usable
            if(swapChain->imageLayouts[imageIdx] == VK_IMAGE_LAYOUT_UNDEFINED) {
                auto barrier = Initializers::InitImageMemoryBarrier(swapChain->images[imageIdx],
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_MEMORY_READ_BIT,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

                vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
                swapChain->imageLayouts[imageIdx] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            }
            if (swapChain->depthImageLayout == VK_IMAGE_LAYOUT_UNDEFINED) {
                auto barrier = Initializers::InitImageMemoryBarrier(swapChain->depthImageAllocation.image,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);

                vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
                swapChain->depthImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            }

            vkCmdBeginRenderPass(commandBuffer, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
            swapChainInUse = swapChain;

            if (clear) ClearAttachments();

        }

        void CommandList::BeginRenderPass(const Ref<RenderPass>& renderPass, const Ref<FrameBuffer>& frameBuffer,
            bool clear, bool autoAdjustImageLayouts) {

            if (autoAdjustImageLayouts) {
                std::vector<VkImageMemoryBarrier> barriers;
                for (uint32_t i = 0; i < MAX_COLOR_ATTACHMENTS; i++) {
                    auto& rpAttachment = renderPass->colorAttachments[i];
                    auto& fbAttachment = frameBuffer->colorAttachments[i];
                    if (!fbAttachment.isValid) continue;
                    if (fbAttachment.image->layout != rpAttachment.initialLayout) {
                        auto barrier = Initializers::InitImageMemoryBarrier(fbAttachment.image->image,
                            fbAttachment.image->layout, rpAttachment.initialLayout, VK_ACCESS_MEMORY_READ_BIT,
                            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

                        barriers.push_back(barrier);
                        fbAttachment.image->layout = rpAttachment.initialLayout;
                    }
                }

                auto& rpAttachment = renderPass->depthAttachment;
                auto& fbAttachment = frameBuffer->depthAttachment;
                if (fbAttachment.isValid && fbAttachment.image->layout != rpAttachment.initialLayout) {
                    auto barrier = Initializers::InitImageMemoryBarrier(fbAttachment.image->image,
                        fbAttachment.image->layout, rpAttachment.initialLayout, VK_ACCESS_MEMORY_READ_BIT,
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);

                    barriers.push_back(barrier);
                    fbAttachment.image->layout = rpAttachment.initialLayout;
                }

                if (barriers.size()) {
                    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                        0, 0, nullptr, 0, nullptr, uint32_t(barriers.size()), barriers.data());
                }
            }

            VkRenderPassBeginInfo rpInfo = {};
            rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpInfo.pNext = nullptr;
            rpInfo.renderPass = renderPass->renderPass;
            rpInfo.renderArea.offset.x = 0;
            rpInfo.renderArea.offset.y = 0;
            rpInfo.renderArea.extent = frameBuffer->extent;
            rpInfo.framebuffer = frameBuffer->frameBuffer;

            std::vector<VkClearValue> clearValues;
            for (auto& attachment : renderPass->colorAttachments) {
                if (!attachment.isValid) continue;
                if (attachment.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {
                    clearValues.push_back(renderPass->colorClearValue);
                }
            }
            auto& attachment = renderPass->depthAttachment;
            if (attachment.isValid && attachment.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {
                clearValues.push_back(renderPass->depthClearValue);
            }

            if (clearValues.size()) {
                rpInfo.clearValueCount = uint32_t(clearValues.size());
                rpInfo.pClearValues = clearValues.data();
            }

            vkCmdBeginRenderPass(commandBuffer, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
            renderPassInUse = renderPass;
            frameBufferInUse = frameBuffer;

            if (clear) ClearAttachments();

        }

        void CommandList::EndRenderPass() {

            vkCmdEndRenderPass(commandBuffer);

            // We need to keep track of the image layouts
            if (swapChainInUse) {
                // TODO...
            }
            if (renderPassInUse) {
                for (uint32_t i = 0; i < MAX_COLOR_ATTACHMENTS; i++) {
                    auto& rpAttachment = renderPassInUse->colorAttachments[i];
                    auto& fbAttachment = frameBufferInUse->colorAttachments[i];
                    if (!fbAttachment.isValid) continue;
                    fbAttachment.image->layout = rpAttachment.outputLayout;
                    fbAttachment.image->accessMask = rpAttachment.outputAccessMask;
                }
                if (frameBufferInUse->depthAttachment.isValid) {
                    auto& rpAttachment = renderPassInUse->depthAttachment;
                    auto& fbAttachment = frameBufferInUse->depthAttachment;
                    fbAttachment.image->layout = rpAttachment.outputLayout;
                    fbAttachment.image->accessMask = rpAttachment.outputAccessMask;
                }
            }

            swapChainInUse = nullptr;
            renderPassInUse = nullptr;
            frameBufferInUse = nullptr;

        }

        void CommandList::BeginQuery(const Ref<QueryPool> &queryPool, uint32_t queryIdx) {

            // TODO...
            switch(queryPool->type) {
                case VK_QUERY_TYPE_TIMESTAMP: vkCmdWriteTimestamp(commandBuffer,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, queryPool->pool, queryIdx); break;
                default: break;
            }

        }

        void CommandList::EndQuery(const Ref<QueryPool> &queryPool, uint32_t queryIdx) {

            // TODO...

        }

        void CommandList::Timestamp(const Ref<QueryPool> &queryPool, uint32_t queryIdx) {

            vkCmdWriteTimestamp(commandBuffer,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, queryPool->pool, queryIdx);

        }

        void CommandList::BindPipeline(const Ref<Pipeline>& pipeline) {

            pipelineInUse = pipeline;

            vkCmdBindPipeline(commandBuffer, pipeline->bindPoint, pipeline->pipeline);

            if (!pipeline->isCompute) {
                // Set default values for viewport/scissor, they are required
                auto extent = GetRenderPassExtent();
                SetViewport(0, 0, extent.width, extent.height);
                SetScissor(0, 0, extent.width, extent.height);
            }

            // Reset previous descriptor data such that a new descriptor
            // set must be provided to the new pipeline
            for (uint32_t i = 0; i < DESCRIPTOR_SET_COUNT; i++)
                descriptorBindingData.changed[i] = true;

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

        void CommandList::ClearAttachments() {

            std::vector<VkClearAttachment> clearAttachments;
            VkClearRect clearRect = {};
            if (swapChainInUse) {
                VkClearAttachment colorClear = {}, depthClear = {};
                colorClear.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                colorClear.clearValue = swapChainInUse->colorClearValue;
                colorClear.colorAttachment = 0;
                depthClear.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                depthClear.clearValue = swapChainInUse->depthClearValue;
                depthClear.colorAttachment = 0;
                clearAttachments.push_back(colorClear);
                clearAttachments.push_back(depthClear);

                clearRect.layerCount = 1;
                clearRect.baseArrayLayer = 0;
                clearRect.rect.offset = { 0, 0 };
                clearRect.rect.extent = swapChainInUse->extent;
            }
            if (renderPassInUse) {
                VkClearAttachment colorClear = {}, depthClear = {};
                colorClear.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                depthClear.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

                for (uint32_t i = 0; i < MAX_COLOR_ATTACHMENTS; i++) {
                    if (!renderPassInUse->colorAttachments[i].isValid) continue;
                    colorClear.clearValue = renderPassInUse->colorClearValue;
                    colorClear.colorAttachment = i;
                    clearAttachments.push_back(colorClear);
                }

                if (renderPassInUse->depthAttachment.isValid) {
                    depthClear.clearValue = renderPassInUse->depthClearValue;
                    depthClear.colorAttachment = 0;
                    clearAttachments.push_back(depthClear);
                }

                clearRect.layerCount = 1;
                clearRect.baseArrayLayer = 0;
                clearRect.rect.offset = { 0, 0 };
                clearRect.rect.extent = frameBufferInUse->extent;
            }

            vkCmdClearAttachments(commandBuffer, uint32_t(clearAttachments.size()),
                clearAttachments.data(), 1, &clearRect);

        }

        void CommandList::PushConstants(PushConstantRange *pushConstantRange, void *data) {

            assert(pipelineInUse && "No pipeline is bound");
            if (!pipelineInUse) return;
            assert(pushConstantRange != nullptr && "Push constant range should not be null");
            if (!pushConstantRange) return;

            vkCmdPushConstants(commandBuffer, pipelineInUse->layout, pushConstantRange->range.stageFlags,
                pushConstantRange->range.offset, pushConstantRange->range.size, data);

        }

        void CommandList::BindIndexBuffer(const Ref<Buffer>& buffer, VkIndexType type) {

            assert(pipelineInUse && "No pipeline is bound");
            if (!pipelineInUse || !buffer->buffer) return;
            assert(buffer->size > 0 && "Invalid buffer size");

            vkCmdBindIndexBuffer(commandBuffer, buffer->buffer, 0, type);

        }

        void CommandList::BindVertexBuffer(const Ref<Buffer>& buffer, uint32_t binding) {

            assert(pipelineInUse && "No pipeline is bound");
            if (!pipelineInUse || !buffer->buffer) return;
            assert(buffer->size > 0 && "Invalid buffer size");

            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(commandBuffer, binding, 1, &buffer->buffer, &offset);

        }

        void CommandList::BindBuffer(const Ref<Buffer>& buffer, uint32_t set, uint32_t binding) {

            assert(set < DESCRIPTOR_SET_COUNT && "Descriptor set not allowed for use");
            assert(binding < BINDINGS_PER_DESCRIPTOR_SET && "The binding point is not allowed for use");
            assert(buffer->size > 0 && "Invalid buffer size");

            // Only uniform buffers support dynamic binding for now
            if (buffer->usageFlags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) {
                if (descriptorBindingData.dynamicBuffers[set][binding].first == buffer.get())
                    return;

                // Since the buffer is partially owned by the device, we can safely get the pointer for this frame
                descriptorBindingData.dynamicBuffers[set][binding] = {buffer.get(), 0};
                descriptorBindingData.buffers[set][binding] = nullptr;
                descriptorBindingData.sampledImages[set][binding] = {nullptr, nullptr};
                descriptorBindingData.images[set][binding] = nullptr;
                descriptorBindingData.changed[set] = true;
            }
            else {
                if (descriptorBindingData.buffers[set][binding] == buffer.get())
                    return;

                // Since the buffer is partially owned by the device, we can safely get the pointer for this frame
                descriptorBindingData.buffers[set][binding] = buffer.get();
                descriptorBindingData.dynamicBuffers[set][binding] = {nullptr, 0};
                descriptorBindingData.sampledImages[set][binding] = {nullptr, nullptr};
                descriptorBindingData.images[set][binding] = nullptr;
                descriptorBindingData.changed[set] = true;
            }

        }

        void CommandList::BindBufferOffset(const Ref<Buffer> &buffer, size_t offset, uint32_t set, uint32_t binding) {

            assert(set < DESCRIPTOR_SET_COUNT && "Descriptor set not allowed for use");
            assert(binding < BINDINGS_PER_DESCRIPTOR_SET && "The binding point is not allowed for use");
            assert(buffer->size > 0 && "Invalid buffer size");
            assert(buffer->usageFlags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT &&
                "Only uniform buffers support dynamic bindings");

            // Only indicate a change of the buffer has changed, not just the offset
            descriptorBindingData.changed[set] |=
                descriptorBindingData.dynamicBuffers[set][binding].first != buffer.get();
            // Since the buffer is partially owned by the device, we can safely get the pointer for this frame
            descriptorBindingData.dynamicBuffers[set][binding] = {buffer.get(), uint32_t(offset)};
            descriptorBindingData.buffers[set][binding] = nullptr;
            descriptorBindingData.sampledImages[set][binding] = {nullptr, nullptr};
            descriptorBindingData.images[set][binding] = nullptr;

        }

        void CommandList::BindBuffer(const Ref<MultiBuffer>& buffer, uint32_t set, uint32_t binding) {

            assert(set < DESCRIPTOR_SET_COUNT && "Descriptor set not allowed for use");
            assert(binding < BINDINGS_PER_DESCRIPTOR_SET && "The binding point is not allowed for use");
            assert(buffer->size > 0 && "Invalid buffer size");

            // Only uniform buffers support dynamic binding for now
            if (buffer->GetCurrent()->usageFlags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) {
                if (descriptorBindingData.dynamicBuffers[set][binding].first == buffer->GetCurrent())
                    return;

                // Since the buffer is partially owned by the device, we can safely get the pointer for this frame
                descriptorBindingData.dynamicBuffers[set][binding] = {buffer->GetCurrent(), 0};
                descriptorBindingData.buffers[set][binding] = nullptr;
                descriptorBindingData.sampledImages[set][binding] = {nullptr, nullptr};
                descriptorBindingData.images[set][binding] = nullptr;
                descriptorBindingData.changed[set] = true;
            }
            else {
                if (descriptorBindingData.buffers[set][binding] == buffer->GetCurrent())
                    return;

                // Since the buffer is partially owned by the device, we can safely get the pointer for this frame
                descriptorBindingData.buffers[set][binding] = buffer->GetCurrent();
                descriptorBindingData.dynamicBuffers[set][binding] = {nullptr, 0};
                descriptorBindingData.sampledImages[set][binding] = {nullptr, nullptr};
                descriptorBindingData.images[set][binding] = nullptr;
                descriptorBindingData.changed[set] = true;
            }
        }

        void CommandList::BindBufferOffset(const Ref<MultiBuffer> &buffer, size_t offset, uint32_t set, uint32_t binding) {

            assert(set < DESCRIPTOR_SET_COUNT && "Descriptor set not allowed for use");
            assert(binding < BINDINGS_PER_DESCRIPTOR_SET && "The binding point is not allowed for use");
            assert(buffer->size > 0 && "Invalid buffer size");
            assert(buffer->GetCurrent()->usageFlags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT &&
                   "Only uniform buffers support dynamic bindings");

            // Only indicate a change of the buffer has changed, not just the offset
            descriptorBindingData.changed[set] |=
                descriptorBindingData.dynamicBuffers[set][binding].first != buffer->GetCurrent();
            // Since the buffer is partially owned by the device, we can safely get the pointer for this frame
            descriptorBindingData.dynamicBuffers[set][binding] = {buffer->GetCurrent(), uint32_t(offset)};
            descriptorBindingData.buffers[set][binding] = nullptr;
            descriptorBindingData.sampledImages[set][binding] = {nullptr, nullptr};
            descriptorBindingData.images[set][binding] = nullptr;

        }

        void CommandList::BindImage(const Ref<Image>& image, uint32_t set, uint32_t binding) {

            assert(set < DESCRIPTOR_SET_COUNT && "Descriptor set not allowed for use");
            assert(binding < BINDINGS_PER_DESCRIPTOR_SET && "The binding point is not allowed for use");

            if (descriptorBindingData.images[set][binding] == image.get())
                return;

            descriptorBindingData.images[set][binding] = image.get();
            descriptorBindingData.buffers[set][binding] = nullptr;
            descriptorBindingData.dynamicBuffers[set][binding] = {nullptr, 0};
            descriptorBindingData.sampledImages[set][binding] = { nullptr, nullptr };
            descriptorBindingData.changed[set] = true;

        }

        void CommandList::BindImage(const Ref<Image>& image, const Ref<Sampler>& sampler, uint32_t set, uint32_t binding) {

            assert(set < DESCRIPTOR_SET_COUNT && "Descriptor set not allowed for use");
            assert(binding < BINDINGS_PER_DESCRIPTOR_SET && "The binding point is not allowed for use");

            if (descriptorBindingData.sampledImages[set][binding].first == image.get() ||
                descriptorBindingData.sampledImages[set][binding].second == sampler.get())
                return;

            descriptorBindingData.sampledImages[set][binding] = { image.get(), sampler.get() };
            descriptorBindingData.buffers[set][binding] = nullptr;
            descriptorBindingData.dynamicBuffers[set][binding] = {nullptr, 0};
            descriptorBindingData.images[set][binding] = nullptr;
            descriptorBindingData.changed[set] = true;

        }

        void CommandList::ResetBindings() {

            descriptorBindingData.Reset();

        }

        void CommandList::ImageMemoryBarrier(const Ref<Image> &image, VkImageLayout newLayout,
            VkAccessFlags newAccessMask, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask) {

            auto barrier = Initializers::InitImageMemoryBarrier(image->image, image->layout,
                newLayout, image->accessMask, newAccessMask, image->aspectFlags);
            barrier.subresourceRange.layerCount = image->layers;
            barrier.subresourceRange.levelCount = image->mipLevels;

            vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0,
                nullptr, 0, nullptr, 1, &barrier);

            image->layout = newLayout;
            image->accessMask = newAccessMask;

        }

        void CommandList::ImageMemoryBarrier(ImageBarrier& barrier, VkPipelineStageFlags srcStageMask,
            VkPipelineStageFlags dstStageMask) {

            vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0,
                nullptr, 0, nullptr, 1, &barrier.barrier);

            barrier.image->layout = barrier.newLayout;
            barrier.image->accessMask = barrier.newAccessMask;

        }

        void CommandList::ImageTransition(const Ref<Image> &image, VkImageLayout newLayout,
            VkAccessFlags newAccessMask) {

            auto barrier = Initializers::InitImageMemoryBarrier(image->image, image->layout,
                newLayout, image->accessMask, newAccessMask, image->aspectFlags);
            barrier.subresourceRange.layerCount = image->layers;
            barrier.subresourceRange.levelCount = image->mipLevels;

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

            image->layout = newLayout;
            image->accessMask = newAccessMask;

        }

        void CommandList::BufferMemoryBarrier(const Ref<Buffer> &buffer, VkAccessFlags newAccessMask,
            VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask) {

            auto barrier = Initializers::InitBufferMemoryBarrier(buffer->buffer,
                buffer->accessMask, newAccessMask);

            vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0,
                nullptr, 1, &barrier, 0, nullptr);

            buffer->accessMask = newAccessMask;

        }

        void CommandList::BufferMemoryBarrier(const Ref<MultiBuffer>& buffer, VkAccessFlags newAccessMask,
            VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask) {

            auto barrier = Initializers::InitBufferMemoryBarrier(buffer->GetCurrent()->buffer,
                buffer->GetCurrent()->accessMask, newAccessMask);

            vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0,
                nullptr, 1, &barrier, 0, nullptr);

            buffer->GetCurrent()->accessMask = newAccessMask;

        }

        void CommandList::BufferMemoryBarrier(BufferBarrier& barrier, VkPipelineStageFlags srcStageMask,
            VkPipelineStageFlags dstStageMask) {

            vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0,
                nullptr, 1, &barrier.barrier, 0, nullptr);

            barrier.buffer->accessMask = barrier.newAccessMask;

        }

        void CommandList::PipelineBarrier(VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask) {

            vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0,
                nullptr, 0, nullptr, 0, nullptr);

        }

        void CommandList::PipelineBarrier(std::vector<ImageBarrier>& imageBarriers,
            std::vector<BufferBarrier>& bufferBarriers, VkPipelineStageFlags srcStageMask,
            VkPipelineStageFlags dstStageMask) {

            // Not so sure about the cost of these vector allocations on the heap
            std::vector<VkImageMemoryBarrier> nativeImageBarriers;
            nativeImageBarriers.reserve(imageBarriers.size());
            for (auto& barrier : imageBarriers) {
                nativeImageBarriers.push_back(barrier.barrier);
            }

            std::vector<VkBufferMemoryBarrier> nativeBufferBarriers;
            nativeBufferBarriers.reserve(bufferBarriers.size());
            for (auto& barrier : bufferBarriers) {
                nativeBufferBarriers.push_back(barrier.barrier);
            }

            vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0,
                nullptr, uint32_t(nativeBufferBarriers.size()), nativeBufferBarriers.data(),
                uint32_t(nativeImageBarriers.size()), nativeImageBarriers.data());

            // Only update image layouts afterwards for clarity
            for (auto& barrier : imageBarriers) {
                barrier.image->layout = barrier.newLayout;
                barrier.image->accessMask = barrier.newAccessMask;
            }

            for (auto& barrier : bufferBarriers) {
                barrier.buffer->accessMask = barrier.newAccessMask;
            }

        }

        void CommandList::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex,
            int32_t vertexOffset, uint32_t firstInstance) {

            assert((swapChainInUse || renderPassInUse) && "No render pass is in use");
            assert(pipelineInUse && "No pipeline is bound");
            if (!pipelineInUse) return;
            assert(indexCount && instanceCount && "Index or instance count should not be zero");

            BindDescriptorSets();

            vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);

        }

        void CommandList::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex,
            uint32_t firstInstance) {

            assert(pipelineInUse && "No pipeline is bound");
            if (!pipelineInUse) return;
            assert(vertexCount && instanceCount && "Index or instance count should not be zero");

            BindDescriptorSets();

            vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);

        }

        void CommandList::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {

            assert(!swapChainInUse && !renderPassInUse && "No render pass should be in use for compute commands");
            assert(pipelineInUse && "No pipeline is bound");
            if (!pipelineInUse) return;
            assert(groupCountX && groupCountY && groupCountZ && "Group counts have to be larger equal to one");

            BindDescriptorSets();

            vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);

        }

        void CommandList::DispatchIndirect(const Ref<Buffer> &buffer, uint32_t offset) {

            assert(!swapChainInUse && !renderPassInUse && "No render pass should be in use for compute commands");
            assert(pipelineInUse && "No pipeline is bound");
            if (!pipelineInUse) return;

            BindDescriptorSets();

            vkCmdDispatchIndirect(commandBuffer, buffer->buffer, VkDeviceSize(offset));

        }

        void CommandList::CopyBuffer(const Ref<Buffer>& srcBuffer, const Ref<Buffer>& dstBuffer) {

            VkBufferCopy copy = {};
            copy.srcOffset = 0;
            copy.dstOffset = 0;
            copy.size = srcBuffer->size;

            CopyBuffer(srcBuffer, dstBuffer, copy);

        }

        void CommandList::CopyBuffer(const Ref<Buffer>& srcBuffer, const Ref<Buffer>& dstBuffer, VkBufferCopy copy) {

            vkCmdCopyBuffer(commandBuffer, srcBuffer->buffer, dstBuffer->buffer, 1, &copy);

        }

        void CommandList::FillBuffer(const Ref<Buffer> &buffer, void *data) {

            // The data has to have a size of 4 bytes and only 4 bytes are taken
            uint32_t word = *static_cast<uint32_t*>(data);
            vkCmdFillBuffer(commandBuffer, buffer->buffer, 0, VK_WHOLE_SIZE, word);

        }

        void CommandList::CopyImage(const Ref<Image>& srcImage,const Ref<Image>& dstImage) {

            VkImageCopy copy = {};
            copy.srcSubresource.aspectMask = srcImage->aspectFlags;
            copy.srcSubresource.layerCount = uint32_t(srcImage->layers);
            copy.dstSubresource.aspectMask = dstImage->aspectFlags;
            copy.dstSubresource.layerCount = uint32_t(dstImage->layers);
            copy.extent = { srcImage->width, srcImage->height, srcImage->depth };

            CopyImage(srcImage, dstImage, copy);

        }

        void CommandList::CopyImage(const Ref<Image>& srcImage, const Ref<Image>& dstImage, VkImageCopy copy) {

            vkCmdCopyImage(commandBuffer, srcImage->image, srcImage->layout,
                dstImage->image, dstImage->layout, 1, &copy);

        }

        void CommandList::BlitImage(const Ref<Image> &srcImage, const Ref<Image> &dstImage) {

            VkImageBlit blit = {};
            blit.srcOffsets[0] = { 0, 0, 0 };
            blit.srcOffsets[1] = { int32_t(srcImage->width), int32_t(srcImage->height),
                                   int32_t(srcImage->depth) };
            blit.srcSubresource.aspectMask = srcImage->aspectFlags;
            blit.srcSubresource.mipLevel = 0;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = srcImage->layers;
            blit.dstOffsets[0] = { 0, 0, 0 };
            blit.dstOffsets[1] = { int32_t(dstImage->width), int32_t(dstImage->height),
                                   int32_t(dstImage->depth) };
            blit.dstSubresource.aspectMask = dstImage->aspectFlags;
            blit.dstSubresource.mipLevel = 0;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = dstImage->layers;

            BlitImage(srcImage, dstImage, blit);

        }

        void CommandList::BlitImage(const Ref<Image> &srcImage, const Ref<Image> &dstImage, VkImageBlit blit) {

            vkCmdBlitImage(commandBuffer, srcImage->image, srcImage->layout, dstImage->image,
                dstImage->layout, 1, &blit, VK_FILTER_LINEAR);

        }

        void CommandList::GenerateMipMap(const Ref<Image> &image) {

            memoryManager->transferManager->GenerateMipMaps(image.get(), commandBuffer);

        }

        void CommandList::BindDescriptorSets() {

            VkWriteDescriptorSet setWrites[2 * BINDINGS_PER_DESCRIPTOR_SET];
            VkDescriptorBufferInfo bufferInfos[2 * BINDINGS_PER_DESCRIPTOR_SET];
            VkDescriptorImageInfo imageInfos[2 * BINDINGS_PER_DESCRIPTOR_SET];

            uint32_t dynamicOffsets[2 * BINDINGS_PER_DESCRIPTOR_SET];

            auto shader = pipelineInUse->shader;

            for (uint32_t i = 0; i < DESCRIPTOR_SET_COUNT; i++) {
                uint32_t dynamicOffsetCounter = 0;

                // We need to collect the dynamic offsets everytime we bind a descriptor set
                // This also means if just the offset changed, we don't need to update the set
                for (uint32_t j = 0; j < BINDINGS_PER_DESCRIPTOR_SET; j++) {
                    if (!descriptorBindingData.dynamicBuffers[i][j].first) continue;
                    const auto& binding = shader->sets[i].bindings[j];
                    // This probably is an old binding, which isn't used by this shader
                    if (!binding.valid) continue;
                    // Check that the descriptor types match up
                    const auto descriptorType = binding.layoutBinding.descriptorType;
                    if (descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC &&
                        descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
                        continue;

                    auto [_, offset] = descriptorBindingData.dynamicBuffers[i][j];

                    dynamicOffsets[dynamicOffsetCounter++] = offset;
                }

                // We could run into issue
                if (descriptorBindingData.changed[i] && shader->sets[i].bindingCount > 0) {
                    uint32_t bindingCounter = 0;
                    descriptorBindingData.changed[i] = false;

                    descriptorBindingData.sets[i] = descriptorPool->Allocate(shader->sets[i].layout);

                    // DYNAMIC BUFFER
                    for (uint32_t j = 0; j < BINDINGS_PER_DESCRIPTOR_SET; j++) {
                        if (!descriptorBindingData.dynamicBuffers[i][j].first) continue;
                        const auto& binding = shader->sets[i].bindings[j];
                        // This probably is an old binding, which isn't used by this shader
                        if (!binding.valid) continue;
                        // Check that the descriptor types match up
                        auto descriptorType = binding.layoutBinding.descriptorType;
                        if (descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC &&
                            descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) continue;

                        auto [buffer, offset] = descriptorBindingData.dynamicBuffers[i][j];

                        auto& bufferInfo = bufferInfos[bindingCounter];
                        bufferInfo.offset = 0;
                        bufferInfo.buffer = buffer->buffer;
                        bufferInfo.range = binding.size ? std::min(binding.size, uint32_t(buffer->size)) : VK_WHOLE_SIZE;

                        auto& setWrite = setWrites[bindingCounter++];
                        setWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        setWrite.pNext = nullptr;
                        setWrite.dstBinding = j;
                        setWrite.dstArrayElement = binding.arrayElement;
                        setWrite.dstSet = descriptorBindingData.sets[i];
                        setWrite.descriptorCount = 1;
                        setWrite.descriptorType = descriptorType;
                        setWrite.pBufferInfo = &bufferInfo;
                    }

                    // BUFFER
                    for (uint32_t j = 0; j < BINDINGS_PER_DESCRIPTOR_SET; j++) {
                        if (!descriptorBindingData.buffers[i][j]) continue;
                        const auto& binding = shader->sets[i].bindings[j];
                        // This probably is an old binding, which isn't used by this shader
                        if (!binding.valid) continue;
                        // Check that the descriptor types match up
                        auto descriptorType = binding.layoutBinding.descriptorType;
                        if (descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) continue;

                        auto buffer = descriptorBindingData.buffers[i][j];

                        auto& bufferInfo = bufferInfos[bindingCounter];
                        bufferInfo.offset = 0;
                        bufferInfo.buffer = buffer->buffer;
                        bufferInfo.range = binding.size ? std::min(binding.size, uint32_t(buffer->size)) : VK_WHOLE_SIZE;

                        auto& setWrite = setWrites[bindingCounter++];
                        setWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        setWrite.pNext = nullptr;
                        setWrite.dstBinding = j;
                        setWrite.dstArrayElement = binding.arrayElement;
                        setWrite.dstSet = descriptorBindingData.sets[i];
                        setWrite.descriptorCount = 1;
                        setWrite.descriptorType = descriptorType;
                        setWrite.pBufferInfo = &bufferInfo;
                    }

                    // SAMPLED IMAGES
                    for (uint32_t j = 0; j < BINDINGS_PER_DESCRIPTOR_SET; j++) {
                        if (!descriptorBindingData.sampledImages[i][j].first ||
                            !descriptorBindingData.sampledImages[i][j].second) continue;
                        const auto& binding = shader->sets[i].bindings[j];
                        // This probably is an old binding, which isn't used by this shader
                        if (!binding.valid) continue;
                        // Check that the descriptor types match up
                        const auto descriptorType = binding.layoutBinding.descriptorType;
                        if (descriptorType != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                            continue;

                        auto [image, sampler] = descriptorBindingData.sampledImages[i][j];

                        auto& imageInfo = imageInfos[bindingCounter];
                        imageInfo.sampler = sampler->sampler;
                        imageInfo.imageView = image->view;
                        imageInfo.imageLayout = image->layout;

                        auto& setWrite = setWrites[bindingCounter++];
                        setWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        setWrite.pNext = nullptr;
                        setWrite.dstBinding = j;
                        setWrite.dstArrayElement = binding.arrayElement;
                        setWrite.dstSet = descriptorBindingData.sets[i];
                        setWrite.descriptorCount = 1;
                        setWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                        setWrite.pImageInfo = &imageInfo;
                    }

                    // STORAGE IMAGES OR IMAGES SEPARATED FROM SAMPLER
                    for (uint32_t j = 0; j < BINDINGS_PER_DESCRIPTOR_SET; j++) {
                        if (!descriptorBindingData.images[i][j]) continue;
                        const auto& binding = shader->sets[i].bindings[j];
                        // This probably is an old binding, which isn't used by this shader
                        // This probably is an old binding, which isn't used by this shader
                        if (!binding.valid) continue;
                        // Check that the descriptor types match up
                        const auto descriptorType = binding.layoutBinding.descriptorType;
                        if (descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                            continue;

                        auto image = descriptorBindingData.images[i][j];

                        auto& imageInfo = imageInfos[bindingCounter];
                        imageInfo.sampler = VK_NULL_HANDLE;
                        imageInfo.imageView = image->view;
                        imageInfo.imageLayout = image->layout;

                        auto& setWrite = setWrites[bindingCounter++];
                        setWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        setWrite.pNext = nullptr;
                        setWrite.dstBinding = j;
                        setWrite.dstArrayElement = binding.arrayElement;
                        setWrite.dstSet = descriptorBindingData.sets[i];
                        setWrite.descriptorCount = 1;
                        setWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                        setWrite.pImageInfo = &imageInfo;
                    }

                    vkUpdateDescriptorSets(device, bindingCounter, setWrites, 0, nullptr);

                }

                if (descriptorBindingData.sets[i] != nullptr && shader->sets[i].bindingCount > 0) {
                    vkCmdBindDescriptorSets(commandBuffer, pipelineInUse->bindPoint,
                        pipelineInUse->layout, i, 1, &descriptorBindingData.sets[i],
                        dynamicOffsetCounter, dynamicOffsets);
                }
            }

        }

        void CommandList::ResetDescriptors() {

            descriptorBindingData.Reset();
            descriptorPool->Reset();

        }

        const VkExtent2D CommandList::GetRenderPassExtent() const {

            VkExtent2D extent = {};

            if (swapChainInUse) {
                extent = swapChainInUse->extent;
            }
            else if (renderPassInUse) {
                extent = frameBufferInUse->extent;
            }
            else {
                assert(0 && "No valid render pass found");
            }

            return extent;

        }

    }

}