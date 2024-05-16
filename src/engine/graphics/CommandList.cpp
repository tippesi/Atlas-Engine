#include "CommandList.h"
#include "GraphicsDevice.h"

#include <cassert>
#include <thread>

namespace Atlas {

    namespace Graphics {

        CommandList::CommandList(GraphicsDevice* device, QueueType queueType, uint32_t queueFamilyIndex,
            std::vector<Ref<Queue>>& queues, bool frameIndependent) : memoryManager(device->memoryManager),
            device(device->device), frameIndependent(frameIndependent), queueType(queueType), queueFamilyIndex(queueFamilyIndex) {

            VkCommandPoolCreateInfo poolCreateInfo = Initializers::InitCommandPoolCreateInfo(queueFamilyIndex);
            VK_CHECK(vkCreateCommandPool(device->device, &poolCreateInfo, nullptr, &commandPool))

            VkCommandBufferAllocateInfo bufferAllocateInfo = Initializers::InitCommandBufferAllocateInfo(commandPool, 1);
            VK_CHECK(vkAllocateCommandBuffers(device->device, &bufferAllocateInfo, &commandBuffer))

            VkFenceCreateInfo fenceInfo = Initializers::InitFenceCreateInfo();
            VK_CHECK(vkCreateFence(device->device, &fenceInfo, nullptr, &fence))

            auto threadId = std::this_thread::get_id();

            for (auto& queue : queues) {
                Semaphore semaphore{
                    .queue = queue->queue
                };

                QueueRef ref(queue, threadId, true);
                VkSemaphoreCreateInfo semaphoreInfo = Initializers::InitSemaphoreCreateInfo();
                VK_CHECK(vkCreateSemaphore(device->device, &semaphoreInfo, nullptr, &semaphore.semaphore));
                ref.Unlock();

                semaphores.push_back(semaphore);
            }

            auto descriptorPoolDesc = DescriptorPoolDesc();
            descriptorPool = new DescriptorPool(device, descriptorPoolDesc);

            CreatePlaceholders(device);

            isComplete = true;

        }

        CommandList::~CommandList() {

            delete descriptorPool;

            for (auto& semaphore : semaphores)
                vkDestroySemaphore(device, semaphore.semaphore, nullptr);

            vkDestroyFence(device, fence, nullptr);
            vkDestroyCommandPool(device, commandPool, nullptr);

        }

        void CommandList::DependsOn(CommandList *commandList) {

            dependencies.push_back(commandList);

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

            // This is only effective for the first time it's called
            ChangePlaceholderLayouts();

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
            if (swapChain->depthImageLayouts[imageIdx] == VK_IMAGE_LAYOUT_UNDEFINED) {
                auto barrier = Initializers::InitImageMemoryBarrier(swapChain->depthImageAllocations[imageIdx].image,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);

                vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
                swapChain->depthImageLayouts[imageIdx] = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            }

            vkCmdBeginRenderPass(commandBuffer, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
            swapChainInUse = swapChain;

            wasSwapChainAccessed = true;

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
                    const auto& rpAttachment = renderPassInUse->colorAttachments[i];
                    const auto& fbAttachment = frameBufferInUse->colorAttachments[i];
                    if (!fbAttachment.isValid) continue;
                    fbAttachment.image->layout = rpAttachment.outputLayout;
                    fbAttachment.image->accessMask = VK_ACCESS_SHADER_READ_BIT;
                }
                if (frameBufferInUse->depthAttachment.isValid) {
                    const auto& rpAttachment = renderPassInUse->depthAttachment;
                    const auto& fbAttachment = frameBufferInUse->depthAttachment;
                    fbAttachment.image->layout = rpAttachment.outputLayout;
                    fbAttachment.image->accessMask = VK_ACCESS_SHADER_READ_BIT;
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

        void CommandList::BeginDebugMarker(const std::string &name, glm::vec4 color) {

            VkDebugMarkerMarkerInfoEXT markerInfo = {};
            markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;

            memcpy(markerInfo.color, glm::value_ptr(color), sizeof(vec4));
            markerInfo.pMarkerName = name.c_str();

            vkCmdDebugMarkerBeginEXT(commandBuffer, &markerInfo);

        }

        void CommandList::EndDebugMarker() {

            vkCmdDebugMarkerEndEXT(commandBuffer);

        }

        void CommandList::BindPipeline(const Ref<Pipeline>& pipeline) {

            // Reset previous descriptor data such that a new descriptor
            // set must be provided to the new pipeline only if descriptor set changes
            if (pipelineInUse != nullptr) {
                for (uint32_t i = 0; i < DESCRIPTOR_SET_COUNT; i++) {
                    bool changed = (descriptorBindingData.layouts[i].get() != pipeline->shader->sets[i].layout.get()
                        && pipeline->shader->sets[i].bindingCount > 0);
                    descriptorBindingData.changed[i] |= changed;

                    if (pipeline->shader->sets[i].bindingCount > 0) {
                        descriptorBindingData.layouts[i] = pipeline->shader->sets[i].layout;
                    }
                }                
            }
            else {
                for (uint32_t i = 0; i < DESCRIPTOR_SET_COUNT; i++)
                    descriptorBindingData.changed[i] = true;
            }
            

            pipelineInUse = pipeline;

            vkCmdBindPipeline(commandBuffer, pipeline->bindPoint, pipeline->pipeline);

            if (!pipeline->isCompute) {
                // Set default values for viewport/scissor, they are required
                auto extent = GetRenderPassExtent();
                SetViewport(0, 0, extent.width, extent.height);
                SetScissor(0, 0, extent.width, extent.height);
            }

        }

        void CommandList::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {

            AE_ASSERT(pipelineInUse && "No pipeline is bound");
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

            AE_ASSERT(pipelineInUse && "No pipeline is bound");
            if (!pipelineInUse) return;

            VkRect2D scissor = {};
            scissor.offset.x = x;
            scissor.offset.y = y;
            scissor.extent.width = width;
            scissor.extent.height = height;

            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        }

        void CommandList::SetLineWidth(float width) {

            AE_ASSERT(pipelineInUse && "No pipeline is bound");
            if (!pipelineInUse) return;

            vkCmdSetLineWidth(commandBuffer, width);

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

        void CommandList::PushConstants(const std::string& pushConstantRangeName, void *data, uint32_t size) {

            AE_ASSERT(pipelineInUse && "No pipeline is bound");
            if (!pipelineInUse) return;

            auto pushConstantRange = pipelineInUse->shader->GetPushConstantRange(pushConstantRangeName);
            if (!pushConstantRange) return;

            size = size != 0 ? size : pushConstantRange->range.size;
            vkCmdPushConstants(commandBuffer, pipelineInUse->layout, pushConstantRange->range.stageFlags,
                pushConstantRange->range.offset, size, data);

        }

        void CommandList::BindIndexBuffer(const Ref<Buffer>& buffer, VkIndexType type) {

            AE_ASSERT(pipelineInUse && "No pipeline is bound");
            if (!pipelineInUse || !buffer->buffer) return;
            AE_ASSERT(buffer->size > 0 && "Invalid buffer size");

            vkCmdBindIndexBuffer(commandBuffer, buffer->buffer, 0, type);

        }

        void CommandList::BindVertexBuffer(const Ref<Buffer>& buffer, uint32_t binding) {

            AE_ASSERT(pipelineInUse && "No pipeline is bound");
            if (!pipelineInUse || !buffer->buffer) return;
            AE_ASSERT(buffer->size > 0 && "Invalid buffer size");

            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(commandBuffer, binding, 1, &buffer->buffer, &offset);

        }

        void CommandList::BindVertexBuffers(std::vector<Ref<Buffer>> &buffers, uint32_t bindingOffset,
            uint32_t bindingCount) {

            AE_ASSERT(pipelineInUse && "No pipeline is bound");
            if (!pipelineInUse) return;

            AE_ASSERT(buffers.size() < MAX_VERTEX_BUFFER_BINDINGS);

            VkDeviceSize offset[MAX_VERTEX_BUFFER_BINDINGS];
            VkBuffer bindBuffers[MAX_VERTEX_BUFFER_BINDINGS];

            for (uint32_t i = 0; i < bindingCount; i++) {
                const auto& buffer = buffers[i];
                if (!buffer->buffer) return;
                AE_ASSERT(buffer->size > 0 && "Invalid buffer size");
                bindBuffers[i] = buffer->buffer;
                offset[i] = 0;
            }

            vkCmdBindVertexBuffers(commandBuffer, bindingOffset, bindingCount, bindBuffers, offset);

        }

        void CommandList::BindBuffer(const Ref<Buffer>& buffer, uint32_t set, uint32_t binding) {

            AE_ASSERT(set < DESCRIPTOR_SET_COUNT && "Descriptor set not allowed for use");
            AE_ASSERT(binding < BINDINGS_PER_DESCRIPTOR_SET && "The binding point is not allowed for use");
            AE_ASSERT(buffer->size > 0 && "Invalid buffer size");

            if (descriptorBindingData.buffers[set][binding].first == buffer.get())
                return;

            descriptorBindingData.ResetBinding(set, binding);

            // Since the buffer is partially owned by the device, we can safely get the pointer for this frame
            descriptorBindingData.buffers[set][binding] = { buffer.get(), 0u };
            descriptorBindingData.changed[set] = true;

        }

        void CommandList::BindBufferOffset(const Ref<Buffer> &buffer, size_t offset, uint32_t set, uint32_t binding) {

            AE_ASSERT(set < DESCRIPTOR_SET_COUNT && "Descriptor set not allowed for use");
            AE_ASSERT(binding < BINDINGS_PER_DESCRIPTOR_SET && "The binding point is not allowed for use");
            AE_ASSERT(buffer->size > 0 && "Invalid buffer size");
            AE_ASSERT(buffer->usageFlags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT &&
                "Only uniform buffers support dynamic bindings");

            if (descriptorBindingData.buffers[set][binding].first == buffer.get())
                return;

            descriptorBindingData.ResetBinding(set, binding);

            // Since the buffer is partially owned by the device, we can safely get the pointer for this frame
            descriptorBindingData.buffers[set][binding] = { buffer.get(), uint32_t(offset) };
            descriptorBindingData.changed[set] = true;

        }

        void CommandList::BindBuffers(const std::vector<Ref<Buffer>> &buffers, uint32_t set, uint32_t binding) {

            AE_ASSERT(set < DESCRIPTOR_SET_COUNT && "Descriptor set not allowed for use");
            AE_ASSERT(binding < BINDINGS_PER_DESCRIPTOR_SET && "The binding point is not allowed for use");
            
            if (!buffers.size()) return;

            std::vector<Buffer*> buffersPtr;
            for (auto& buffer : buffers) {
                AE_ASSERT(buffer->size > 0 && "Invalid buffer size");
                AE_ASSERT(buffer->usageFlags & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT &&
                       "Only storage buffers support array bindings");

                buffersPtr.push_back(buffer.get());
            }

            descriptorBindingData.ResetBinding(set, binding);

            // Since the buffer is partially owned by the device, we can safely get the pointer for this frame
            descriptorBindingData.buffersArray[set][binding] = { buffersPtr };
            descriptorBindingData.changed[set] = true;

        }

        void CommandList::BindBuffer(const Ref<MultiBuffer>& buffer, uint32_t set, uint32_t binding) {

            AE_ASSERT(set < DESCRIPTOR_SET_COUNT && "Descriptor set not allowed for use");
            AE_ASSERT(binding < BINDINGS_PER_DESCRIPTOR_SET && "The binding point is not allowed for use");
            AE_ASSERT(buffer->size > 0 && "Invalid buffer size");

            if (descriptorBindingData.buffers[set][binding].first == buffer->GetCurrent())
                return;

            descriptorBindingData.ResetBinding(set, binding);

            // Since the buffer is partially owned by the device, we can safely get the pointer for this frame
            descriptorBindingData.buffers[set][binding] = { buffer->GetCurrent(), 0 };
            descriptorBindingData.changed[set] = true;
        }

        void CommandList::BindBufferOffset(const Ref<MultiBuffer> &buffer, size_t offset, uint32_t set, uint32_t binding) {

            AE_ASSERT(set < DESCRIPTOR_SET_COUNT && "Descriptor set not allowed for use");
            AE_ASSERT(binding < BINDINGS_PER_DESCRIPTOR_SET && "The binding point is not allowed for use");
            AE_ASSERT(buffer->size > 0 && "Invalid buffer size");
            AE_ASSERT(buffer->GetCurrent()->usageFlags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT &&
                   "Only uniform buffers support dynamic bindings");

            // Need to check before reset
            bool changed = descriptorBindingData.buffers[set][binding].first != buffer->GetCurrent();
            bool offsetChanged = descriptorBindingData.buffers[set][binding].second != uint32_t(offset);

            descriptorBindingData.ResetBinding(set, binding);

            // Only indicate a change of the buffer has changed, not just the offset
            descriptorBindingData.changed[set] |= changed;

            // Since the buffer is partially owned by the device, we can safely get the pointer for this frame
            descriptorBindingData.buffers[set][binding] = {buffer->GetCurrent(), uint32_t(offset)};

        }

        void CommandList::BindImage(const Ref<Image>& image, uint32_t set, uint32_t binding, int32_t mipLevel) {

            AE_ASSERT(set < DESCRIPTOR_SET_COUNT && "Descriptor set not allowed for use");
            AE_ASSERT(binding < BINDINGS_PER_DESCRIPTOR_SET && "The binding point is not allowed for use");
            AE_ASSERT((mipLevel < 0 || mipLevel < image->mipLevels) && "Invalid mip level selected");

            if (descriptorBindingData.images[set][binding].first == image.get() &&
                descriptorBindingData.images[set][binding].second == mipLevel)
                return;

            descriptorBindingData.ResetBinding(set, binding);

            descriptorBindingData.images[set][binding] = { image.get(), mipLevel };
            descriptorBindingData.changed[set] = true;

        }

        void CommandList::BindImage(const Ref<Image>& image, const Ref<Sampler>& sampler, uint32_t set, uint32_t binding) {

            AE_ASSERT(set < DESCRIPTOR_SET_COUNT && "Descriptor set not allowed for use");
            AE_ASSERT(binding < BINDINGS_PER_DESCRIPTOR_SET && "The binding point is not allowed for use");

            if (descriptorBindingData.sampledImages[set][binding].first == image.get() &&
                descriptorBindingData.sampledImages[set][binding].second == sampler.get())
                return;

            descriptorBindingData.ResetBinding(set, binding);

            descriptorBindingData.sampledImages[set][binding] = { image.get(), sampler.get() };
            descriptorBindingData.changed[set] = true;

        }

        void CommandList::BindSampledImages(const std::vector<Ref<Image>>& images, uint32_t set, uint32_t binding) {

            AE_ASSERT(set < DESCRIPTOR_SET_COUNT && "Descriptor set not allowed for use");
            AE_ASSERT(binding < BINDINGS_PER_DESCRIPTOR_SET && "The binding point is not allowed for use");

            if (!images.size()) return;

            std::vector<Image*> imagesPtr;
            for (const auto& image : images) imagesPtr.push_back(image.get());

            descriptorBindingData.ResetBinding(set, binding);

            // We don't do any checks here if the same things were bound already
            descriptorBindingData.sampledImagesArray[set][binding] = { imagesPtr };
            descriptorBindingData.changed[set] = true;

        }

        void CommandList::BindSampler(const Ref<Sampler>& sampler, uint32_t set, uint32_t binding) {

            AE_ASSERT(set < DESCRIPTOR_SET_COUNT && "Descriptor set not allowed for use");
            AE_ASSERT(binding < BINDINGS_PER_DESCRIPTOR_SET && "The binding point is not allowed for use");

            if (descriptorBindingData.samplers[set][binding] == sampler.get())
                return;

            descriptorBindingData.ResetBinding(set, binding);

            descriptorBindingData.samplers[set][binding] = sampler.get();
            descriptorBindingData.changed[set] = true;

        }

        void CommandList::BindTLAS(const Ref<Atlas::Graphics::TLAS> &tlas, uint32_t set, uint32_t binding) {

            AE_ASSERT(set < DESCRIPTOR_SET_COUNT && "Descriptor set not allowed for use");
            AE_ASSERT(binding < BINDINGS_PER_DESCRIPTOR_SET && "The binding point is not allowed for use");

            if (descriptorBindingData.tlases[set][binding] == tlas.get())
                return;

            descriptorBindingData.ResetBinding(set, binding);

            descriptorBindingData.tlases[set][binding] = tlas.get();
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

        }

        void CommandList::MemoryBarrier(VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
            VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask) {

            VkMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            barrier.srcAccessMask = srcAccessMask;
            barrier.dstAccessMask = dstAccessMask;

            vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 1,
                &barrier, 0, nullptr, 0, nullptr);

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

        }

        void CommandList::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex,
            int32_t vertexOffset, uint32_t firstInstance) {

            AE_ASSERT((swapChainInUse || renderPassInUse) && "No render pass is in use");
            AE_ASSERT(pipelineInUse && "No pipeline is bound");
            if (!pipelineInUse) return;
            AE_ASSERT(indexCount && instanceCount && "Index or instance count should not be zero");

            BindDescriptorSets();

            vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);

        }

        void CommandList::DrawIndexedIndirect(const Ref<Graphics::Buffer> &buffer, size_t offset,
            uint32_t drawCount, uint32_t stride) {

            AE_ASSERT((swapChainInUse || renderPassInUse) && "No render pass is in use");
            AE_ASSERT(pipelineInUse && "No pipeline is bound");
            if (!pipelineInUse) return;

            BindDescriptorSets();

            vkCmdDrawIndexedIndirect(commandBuffer, buffer->buffer, offset, drawCount, stride);

        }

        void CommandList::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex,
            uint32_t firstInstance) {

            AE_ASSERT(pipelineInUse && "No pipeline is bound");
            if (!pipelineInUse) return;
            AE_ASSERT(vertexCount && instanceCount && "Index or instance count should not be zero");

            BindDescriptorSets();

            vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);

        }

        void CommandList::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {

            AE_ASSERT(!swapChainInUse && !renderPassInUse && "No render pass should be in use for compute commands");
            AE_ASSERT(pipelineInUse && "No pipeline is bound");
            if (!pipelineInUse) return;
            AE_ASSERT(groupCountX && groupCountY && groupCountZ && "Group counts have to be larger equal to one");

            BindDescriptorSets();

            vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);

        }

        void CommandList::DispatchIndirect(const Ref<Buffer> &buffer, uint32_t offset) {

            AE_ASSERT(!swapChainInUse && !renderPassInUse && "No render pass should be in use for compute commands");
            AE_ASSERT(pipelineInUse && "No pipeline is bound");
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

        void CommandList::FillBuffer(const Ref<MultiBuffer> &buffer, void *data) {

            // The data has to have a size of 4 bytes and only 4 bytes are taken
            uint32_t word = *static_cast<uint32_t*>(data);
            vkCmdFillBuffer(commandBuffer, buffer->GetCurrent()->buffer, 0, VK_WHOLE_SIZE, word);

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

        void CommandList::ClearImageColor(const Ref<Image>& image, VkClearColorValue clearColor) {

            AE_ASSERT((image->aspectFlags & VK_IMAGE_ASPECT_COLOR_BIT) && "Image needs to have VK_IMAGE_ASPECT_COLOR_BIT set in aspect mask");

            VkImageSubresourceRange range = {};
			range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			range.baseMipLevel = 0;
			range.levelCount = image->mipLevels;
			range.baseArrayLayer = 0;
			range.layerCount = image->layers;

            vkCmdClearColorImage(commandBuffer, image->image, image->layout, &clearColor, 1, &range);

        }

        void CommandList::GenerateMipMaps(const Ref<Image> &image) {

            memoryManager->transferManager->GenerateMipMaps(image.get(), commandBuffer);

        }

        void CommandList::BuildBLAS(const Ref<BLAS> &blas, VkAccelerationStructureBuildGeometryInfoKHR& buildInfo) {

            vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, &blas->rangeInfo);

        }

        void CommandList::BuildTLAS(const Ref<TLAS> &tlas, VkAccelerationStructureBuildGeometryInfoKHR& buildInfo) {

            vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, &tlas->rangeInfo);

        }

        void CommandList::BindDescriptorSets() {

            auto& setWrites = descriptorBindingData.setWrites;
            auto& bufferInfos = descriptorBindingData.bufferInfos;
            auto& imageInfos = descriptorBindingData.imageInfos;
            auto& tlasInfos = descriptorBindingData.tlasInfos;

            uint32_t dynamicOffsets[BINDINGS_PER_DESCRIPTOR_SET];

            auto shader = pipelineInUse->shader;

            for (uint32_t i = 0; i < DESCRIPTOR_SET_COUNT; i++) {
                uint32_t dynamicOffsetCounter = 0;

                // We need to collect the dynamic offsets everytime we bind a descriptor set
                // This also means if just the offset changed, we don't need to update the set
                for (uint32_t j = 0; j < BINDINGS_PER_DESCRIPTOR_SET; j++) {
                    if (!descriptorBindingData.buffers[i][j].first) continue;
                    const auto& binding = shader->sets[i].bindings[j];
                    // This probably is an old binding, which isn't used by this shader
                    if (!binding.valid) continue;
                    // Check that the descriptor types match up
                    const auto descriptorType = binding.binding.descriptorType;
                    if (descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC &&
                        descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
                        continue;

                    auto [_, offset] = descriptorBindingData.buffers[i][j];

                    dynamicOffsets[dynamicOffsetCounter++] = offset;
                }

                // We could run into issue
                if (descriptorBindingData.changed[i] && shader->sets[i].bindingCount > 0) {
                    uint32_t bindingCounter = 0;
                    uint32_t imageInfoCounter = 0;
                    uint32_t bufferInfoCounter = 0;
                    descriptorBindingData.changed[i] = false;

                    descriptorBindingData.sets[i] = descriptorPool->GetCachedSet(shader->sets[i].layout);

                    // BUFFER
                    for (uint32_t j = 0; j < BINDINGS_PER_DESCRIPTOR_SET; j++) {
                        if (!descriptorBindingData.buffers[i][j].first) continue;
                        const auto& shaderBinding = shader->sets[i].bindings[j];
                        // This probably is an old binding, which isn't used by this shader
                        if (!shaderBinding.valid) continue;

                        const auto& binding = shaderBinding.binding;
                        // Check that the descriptor types match up
                        auto descriptorType = binding.descriptorType;
                        if (descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER &&
                            descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER &&
                            descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC &&
                            descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) continue;

                        auto [buffer, _] = descriptorBindingData.buffers[i][j];

                        auto& bufferInfo = bufferInfos[bufferInfoCounter++];
                        bufferInfo.offset = 0;
                        bufferInfo.buffer = buffer->buffer;
                        bufferInfo.range = binding.size ? std::min(binding.size, uint64_t(buffer->size)) : VK_WHOLE_SIZE;

                        auto& setWrite = setWrites[bindingCounter++];
                        setWrite = {};
                        setWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        setWrite.pNext = nullptr;
                        setWrite.dstBinding = j;
                        setWrite.dstArrayElement = binding.arrayElement;
                        setWrite.dstSet = descriptorBindingData.sets[i]->set;
                        setWrite.descriptorCount = 1;
                        setWrite.descriptorType = descriptorType;
                        setWrite.pBufferInfo = &bufferInfo;
                    }

                    // SAMPLED IMAGES
                    for (uint32_t j = 0; j < BINDINGS_PER_DESCRIPTOR_SET; j++) {
                        if (!descriptorBindingData.sampledImages[i][j].first ||
                            !descriptorBindingData.sampledImages[i][j].second) continue;
                        const auto& shaderBinding = shader->sets[i].bindings[j];
                        // This probably is an old binding, which isn't used by this shader
                        if (!shaderBinding.valid) continue;

                        const auto& binding = shaderBinding.binding;
                        // Check that the descriptor types match up
                        const auto descriptorType = binding.descriptorType;
                        if (descriptorType != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                            continue;

                        auto [image, sampler] = descriptorBindingData.sampledImages[i][j];

                        auto& imageInfo = imageInfos[imageInfoCounter++];
                        imageInfo.sampler = sampler->sampler;
                        imageInfo.imageView = image->view;
                        imageInfo.imageLayout = image->layout;

                        auto& setWrite = setWrites[bindingCounter++];
                        setWrite = {};
                        setWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        setWrite.pNext = nullptr;
                        setWrite.dstBinding = j;
                        setWrite.dstArrayElement = binding.arrayElement;
                        setWrite.dstSet = descriptorBindingData.sets[i]->set;
                        setWrite.descriptorCount = 1;
                        setWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                        setWrite.pImageInfo = &imageInfo;
                    }

                    // STORAGE IMAGES OR IMAGES SEPARATED FROM SAMPLER
                    for (uint32_t j = 0; j < BINDINGS_PER_DESCRIPTOR_SET; j++) {
                        if (!descriptorBindingData.images[i][j].first) continue;
                        const auto& shaderBinding = shader->sets[i].bindings[j];
                        // This probably is an old binding, which isn't used by this shader
                        if (!shaderBinding.valid) continue;

                        const auto& binding = shaderBinding.binding;
                        // Check that the descriptor types match up
                        const auto descriptorType = binding.descriptorType;
                        if (descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_IMAGE &&
                            descriptorType != VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
                            continue;

                        auto [image, mipLevel] = descriptorBindingData.images[i][j];

                        auto& imageInfo = imageInfos[imageInfoCounter++];
                        imageInfo.sampler = VK_NULL_HANDLE;
                        imageInfo.imageView = mipLevel < 0 ? image->view : image->mipMapViews[mipLevel];
                        imageInfo.imageLayout = image->layout;

                        auto& setWrite = setWrites[bindingCounter++];
                        setWrite = {};
                        setWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        setWrite.pNext = nullptr;
                        setWrite.dstBinding = j;
                        setWrite.dstArrayElement = binding.arrayElement;
                        setWrite.dstSet = descriptorBindingData.sets[i]->set;
                        setWrite.descriptorCount = 1;
                        setWrite.descriptorType = descriptorType;
                        setWrite.pImageInfo = &imageInfo;
                    }

                    // SAMPLERS
                    for (uint32_t j = 0; j < BINDINGS_PER_DESCRIPTOR_SET; j++) {
                        if (!descriptorBindingData.samplers[i][j]) continue;
                        const auto& shaderBinding = shader->sets[i].bindings[j];
                        // This probably is an old binding, which isn't used by this shader
                        if (!shaderBinding.valid) continue;

                        const auto& binding = shaderBinding.binding;
                        // Check that the descriptor types match up
                        const auto descriptorType = binding.descriptorType;
                        if (descriptorType != VK_DESCRIPTOR_TYPE_SAMPLER)
                            continue;

                        auto sampler = descriptorBindingData.samplers[i][j];

                        auto& imageInfo = imageInfos[imageInfoCounter++];
                        imageInfo.sampler = sampler->sampler;
                        imageInfo.imageView = VK_NULL_HANDLE;
                        imageInfo.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

                        auto& setWrite = setWrites[bindingCounter++];
                        setWrite = {};
                        setWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        setWrite.dstBinding = j;
                        setWrite.dstArrayElement = binding.arrayElement;
                        setWrite.dstSet = descriptorBindingData.sets[i]->set;
                        setWrite.descriptorCount = 1;
                        setWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
                        setWrite.pImageInfo = &imageInfo;
                    }

                    // TOP-LEVEL ACCELERATION STRUCTURES
                    for (uint32_t j = 0; j < BINDINGS_PER_DESCRIPTOR_SET; j++) {
                        if (!descriptorBindingData.tlases[i][j]) continue;
                        const auto& shaderBinding = shader->sets[i].bindings[j];
                        // This probably is an old binding, which isn't used by this shader
                        if (!shaderBinding.valid) continue;

                        const auto& binding = shaderBinding.binding;
                        // Check that the descriptor types match up
                        const auto descriptorType = binding.descriptorType;
                        if (descriptorType != VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
                            continue;

                        auto tlas = descriptorBindingData.tlases[i][j];

                        auto& tlasInfo = tlasInfos[bindingCounter];
                        tlasInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
                        tlasInfo.pNext = nullptr;
                        tlasInfo.accelerationStructureCount = 1;
                        tlasInfo.pAccelerationStructures = &tlas->accelerationStructure;

                        auto& setWrite = setWrites[bindingCounter++];
                        setWrite = {};
                        setWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        setWrite.dstBinding = j;
                        setWrite.dstArrayElement = binding.arrayElement;
                        setWrite.dstSet = descriptorBindingData.sets[i]->set;
                        setWrite.descriptorCount = 1;
                        setWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
                        setWrite.pNext = &tlasInfo;
                    }

                    // SAMPLED IMAGE ARRAYS
                    for (uint32_t j = 0; j < BINDINGS_PER_DESCRIPTOR_SET; j++) {
                        auto descriptorArraySize = descriptorBindingData.sets[i]->sampledImageArraySize[j];

                        if (descriptorBindingData.sampledImagesArray[i][j].empty() && !descriptorArraySize) continue;
                        const auto& shaderBinding = shader->sets[i].bindings[j];
                        // This probably is an old binding, which isn't used by this shader
                        if (!shaderBinding.valid) continue;

                        const auto& binding = shaderBinding.binding;
                        // Check that the descriptor types match up
                        const auto descriptorType = binding.descriptorType;
                        if (descriptorType != VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
                            continue;

                        auto sampledImageArraySize = uint32_t(descriptorBindingData.sampledImagesArray[i][j].size());
                        AE_ASSERT(size_t(imageInfoCounter + sampledImageArraySize) < imageInfos.size() &&
                            "Too much data written into buffer infos for descriptor set update");

                        if (size_t(imageInfoCounter) + sampledImageArraySize >= imageInfos.size() ||
                            (sampledImageArraySize == 0 && descriptorArraySize == 0))
                            continue;

                        auto& setWrite = setWrites[bindingCounter++];
                        setWrite = {};
                        setWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        setWrite.pNext = nullptr;
                        setWrite.dstBinding = j;
                        setWrite.dstArrayElement = 0;
                        setWrite.dstSet = descriptorBindingData.sets[i]->set;
                        setWrite.descriptorCount = std::max(sampledImageArraySize, descriptorArraySize);
                        setWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                        setWrite.pImageInfo = &imageInfos[imageInfoCounter];

                        for (uint32_t k = 0; k < sampledImageArraySize; k++) {
                            auto image = descriptorBindingData.sampledImagesArray[i][j][k];

                            auto& imageInfo = imageInfos[imageInfoCounter++];
                            imageInfo.sampler = VK_NULL_HANDLE;
                            imageInfo.imageView = image->view;
                            imageInfo.imageLayout = image->layout;
                        }

                        // We need to overwrite old descriptor binding data. If we don't it might
                        // lead to crashes since resource in descriptor set are deleted already
                        if (sampledImageArraySize < descriptorBindingData.sets[i]->sampledImageArraySize[j]) {
                            for (uint32_t k = sampledImageArraySize; k < descriptorArraySize; k++) {
                                auto& imageInfo = imageInfos[imageInfoCounter++];
                                imageInfo.sampler = VK_NULL_HANDLE;
                                imageInfo.imageView = placeholderImage->view;
                                imageInfo.imageLayout = placeholderImage->layout;
                            }
                        }

                        descriptorBindingData.sets[i]->sampledImageArraySize[j] = sampledImageArraySize;
                    }

                    // BUFFER ARRAYS
                    for (uint32_t j = 0; j < BINDINGS_PER_DESCRIPTOR_SET; j++) {
                        auto descriptorArraySize = descriptorBindingData.sets[i]->bufferArraySize[j];

                        if (descriptorBindingData.buffersArray[i][j].empty() && !descriptorArraySize) continue;
                        const auto& shaderBinding = shader->sets[i].bindings[j];
                        // This probably is an old binding, which isn't used by this shader
                        if (!shaderBinding.valid) continue;

                        const auto& binding = shaderBinding.binding;
                        // Check that the descriptor types match up
                        const auto descriptorType = binding.descriptorType;
                        if (descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                            continue;

                        auto bufferArraySize = uint32_t(descriptorBindingData.buffersArray[i][j].size());
                        AE_ASSERT(size_t(bufferInfoCounter + bufferArraySize) < bufferInfos.size() &&
                            "Too much data written into buffer infos for descriptor set update");

                        if (size_t(bufferInfoCounter) + bufferArraySize >= bufferInfos.size() ||
                            (bufferArraySize == 0 && descriptorArraySize == 0))
                            continue;

                        auto& setWrite = setWrites[bindingCounter++];
                        setWrite = {};
                        setWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        setWrite.pNext = nullptr;
                        setWrite.dstBinding = j;
                        setWrite.dstArrayElement = 0;
                        setWrite.dstSet = descriptorBindingData.sets[i]->set;
                        setWrite.descriptorCount = std::max(bufferArraySize, descriptorArraySize);;
                        setWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                        setWrite.pBufferInfo = &bufferInfos[bufferInfoCounter];

                        for (uint32_t k = 0; k < bufferArraySize; k++) {
                            auto buffer = descriptorBindingData.buffersArray[i][j][k];

                            auto& bufferInfo = bufferInfos[bufferInfoCounter++];
                            bufferInfo.offset = 0;
                            bufferInfo.buffer = buffer->buffer;
                            bufferInfo.range = VK_WHOLE_SIZE;
                        }

                        // We need to overwrite old descriptor binding data. If we don't it might
                        // lead to crashes since resource in descriptor set are deleted already
                        if (bufferArraySize < descriptorBindingData.sets[i]->bufferArraySize[j]) {
                            for (uint32_t k = bufferArraySize; k < descriptorArraySize; k++) {
                                auto& bufferInfo = bufferInfos[bufferInfoCounter++];
                                bufferInfo.offset = 0;
                                bufferInfo.buffer = placeholderBuffer->buffer;
                                bufferInfo.range = VK_WHOLE_SIZE;
                            }
                        }

                        descriptorBindingData.sets[i]->bufferArraySize[j] = bufferArraySize;
                    }

                    vkUpdateDescriptorSets(device, bindingCounter, setWrites.data(), 0, nullptr);

                }

                if (descriptorBindingData.sets[i] != nullptr && shader->sets[i].bindingCount > 0) {
                    vkCmdBindDescriptorSets(commandBuffer, pipelineInUse->bindPoint,
                        pipelineInUse->layout, i, 1, &descriptorBindingData.sets[i]->set,
                        dynamicOffsetCounter, dynamicOffsets);
                }
            }

        }

        void CommandList::ResetDescriptors() {

            descriptorBindingData.Reset();
            descriptorPool->ResetAllocationCounters();

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
                AE_ASSERT(0 && "No valid render pass found");
            }

            return extent;

        }

        const VkSemaphore CommandList::GetSemaphore(VkQueue queue) {

            for (const auto& semaphore : semaphores) {
                if (semaphore.queue == queue)
                    return semaphore.semaphore;
            }

            AE_ASSERT(0 && "Queue not found in available semaphores");

            return semaphores.front().semaphore;

        }

        const std::vector<VkSemaphore> CommandList::GetSemaphores() const {

            std::vector<VkSemaphore> resultSemaphores;

            for (auto& semaphore : semaphores)
                resultSemaphores.push_back(semaphore.semaphore);

            return resultSemaphores;

        }

        void CommandList::CreatePlaceholders(GraphicsDevice* device) {

            auto bufferDesc = Graphics::BufferDesc {
                .usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .size = sizeof(int)
            };
            placeholderBuffer =  device->CreateBuffer(bufferDesc);

            auto imageDesc = Graphics::ImageDesc {
                .usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                .width = 1,
                .height = 1,
                .format = VK_FORMAT_R8G8B8A8_UNORM,
            };
            placeholderImage = device->CreateImage(imageDesc);

        }

        void CommandList::ChangePlaceholderLayouts() {

            // Only ever do this once after command list creation
            if (placeholderImage->layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
                ImageTransition(placeholderImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);
            }

        }

    }

}
