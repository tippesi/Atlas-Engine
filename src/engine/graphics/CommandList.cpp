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

        void CommandList::BeginRenderPass(const Ref<RenderPass>& renderPass, bool clear,
            uint32_t layer, bool autoAdjustImageLayouts) {

            assert(layer < uint32_t(renderPass->frameBuffers.size()) && "Layer not available");

            if (autoAdjustImageLayouts) {
                std::vector<VkImageMemoryBarrier> barriers;
                for (auto &attachment: renderPass->colorAttachments) {
                    if (!attachment.image) continue;
                    if (attachment.image->layout != attachment.initialLayout) {
                        auto barrier = Initializers::InitImageMemoryBarrier(attachment.image->image,
                            attachment.image->layout, attachment.initialLayout, VK_ACCESS_MEMORY_READ_BIT,
                            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

                        barriers.push_back(barrier);
                        attachment.image->layout = attachment.initialLayout;
                    }
                }

                auto& attachment = renderPass->depthAttachment;
                if (attachment.image && attachment.image->layout != attachment.initialLayout) {
                    auto barrier = Initializers::InitImageMemoryBarrier(attachment.image->image,
                        attachment.image->layout, attachment.initialLayout, VK_ACCESS_MEMORY_READ_BIT,
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);

                    barriers.push_back(barrier);
                    attachment.image->layout = attachment.initialLayout;
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
            rpInfo.renderArea.extent = renderPass->extent;
            rpInfo.framebuffer = renderPass->frameBuffers[layer];

            std::vector<VkClearValue> clearValues;
            for (auto& attachment : renderPass->colorAttachments) {
                if (!attachment.image) continue;
                if (attachment.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {
                    clearValues.push_back(renderPass->colorClearValue);
                }
            }
            auto& attachment = renderPass->depthAttachment;
            if (attachment.image && attachment.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {
                clearValues.push_back(renderPass->depthClearValue);
            }

            if (clearValues.size()) {
                rpInfo.clearValueCount = uint32_t(clearValues.size());
                rpInfo.pClearValues = clearValues.data();
            }

            vkCmdBeginRenderPass(commandBuffer, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
            renderPassInUse = renderPass;

            if (clear) ClearAttachments();

        }

        void CommandList::EndRenderPass() {

            vkCmdEndRenderPass(commandBuffer);

            // We need to keep track of the image layouts
            if (swapChainInUse) {
                // TODO...
            }
            if (renderPassInUse) {
                for (auto& attachment : renderPassInUse->colorAttachments) {
                    if (!attachment.image) continue;
                    attachment.image->layout = attachment.outputLayout;
                    attachment.image->accessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                }
                if (renderPassInUse->depthAttachment.image) {
                    auto& attachment = renderPassInUse->depthAttachment;
                    attachment.image->layout = attachment.outputLayout;
                    attachment.image->accessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                }
            }

            swapChainInUse = nullptr;
            renderPassInUse = nullptr;

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

            // Not sure if this will be really needed
            descriptorBindingData.Reset();
            prevDescriptorBindingData.Reset();

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
                    if (!renderPassInUse->colorAttachments[i].image) continue;
                    colorClear.clearValue = renderPassInUse->colorClearValue;
                    colorClear.colorAttachment = i;
                    clearAttachments.push_back(colorClear);
                }

                if (renderPassInUse->depthAttachment.image) {
                    depthClear.clearValue = renderPassInUse->depthClearValue;
                    depthClear.colorAttachment = 0;
                    clearAttachments.push_back(depthClear);
                }

                clearRect.layerCount = 1;
                clearRect.baseArrayLayer = 0;
                clearRect.rect.offset = { 0, 0 };
                clearRect.rect.extent = renderPassInUse->extent;
            }

            vkCmdClearAttachments(commandBuffer, uint32_t(clearAttachments.size()),
                clearAttachments.data(), 1, &clearRect);

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

        void CommandList::BindBuffer(const Ref<Buffer>& buffer, uint32_t set, uint32_t binding) {

            assert(set < DESCRIPTOR_SET_COUNT && "Descriptor set not allowed for use");
            assert(binding < BINDINGS_PER_DESCRIPTOR_SET && "The binding point is not allowed for use");

            // Since the buffer is partially owned by the device, we can safely get the pointer for this frame
            descriptorBindingData.buffers[set][binding] = buffer.get();

        }

        void CommandList::BindBuffer(const Ref<MultiBuffer>& buffer, uint32_t set, uint32_t binding) {

            assert(set < DESCRIPTOR_SET_COUNT && "Descriptor set not allowed for use");
            assert(binding < BINDINGS_PER_DESCRIPTOR_SET && "The binding point is not allowed for use");

            // Since the buffer is partially owned by the device, we can safely get the pointer for this frame
            descriptorBindingData.buffers[set][binding] = buffer->GetCurrent();

        }

        void CommandList::BindImage(const Ref<Image>& image, uint32_t set, uint32_t binding) {

            assert(set < DESCRIPTOR_SET_COUNT && "Descriptor set not allowed for use");
            assert(binding < BINDINGS_PER_DESCRIPTOR_SET && "The binding point is not allowed for use");

            descriptorBindingData.images[set][binding] = image.get();

        }

        void CommandList::BindImage(const Ref<Image>& image, Ref<Sampler>& sampler, uint32_t set, uint32_t binding) {

            assert(set < DESCRIPTOR_SET_COUNT && "Descriptor set not allowed for use");
            assert(binding < BINDINGS_PER_DESCRIPTOR_SET && "The binding point is not allowed for use");

            descriptorBindingData.sampledImages[set][binding] = { image.get(), sampler.get() };

        }

        void CommandList::ResetBindings() {

            descriptorBindingData.Reset();

        }

        void CommandList::ImageMemoryBarrier(ImageBarrier& barrier, VkPipelineStageFlags srcStageMask,
            VkPipelineStageFlags dstStageMask) {

            vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0,
                nullptr, 0, nullptr, 1, &barrier.barrier);

            barrier.image->layout = barrier.newLayout;
            barrier.image->accessMask = barrier.newAccessMask;

        }

        void CommandList::BufferMemoryBarrier(BufferBarrier& barrier, VkPipelineStageFlags srcStageMask,
            VkPipelineStageFlags dstStageMask) {

            vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0,
                nullptr, 1, &barrier.barrier, 0, nullptr);

            barrier.buffer->accessMask = barrier.newAccessMask;

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

                // TODO: Differantiate between uniform and storage buffer
                // Only uniform buffer should have a dynamic offset
                for (uint32_t j = 0; j < BINDINGS_PER_DESCRIPTOR_SET; j++) {
                    if (!descriptorBindingData.buffers[i][j]) continue;
                    dynamicOffsets[dynamicOffsetCounter++] = 0;
                }

                if (!prevDescriptorBindingData.IsEqual(descriptorBindingData, i)) {
                    uint32_t bindingCounter = 0;

                    descriptorBindingData.sets[i] = descriptorPool->Allocate(shader->sets[i].layout);

                    // BUFFER
                    for (uint32_t j = 0; j < BINDINGS_PER_DESCRIPTOR_SET; j++) {
                        if (!descriptorBindingData.buffers[i][j]) continue;
                        const auto& binding = shader->sets[i].bindings[j];

                        auto& bufferInfo = bufferInfos[bindingCounter];
                        bufferInfo.offset = 0;
                        bufferInfo.buffer = descriptorBindingData.buffers[i][j]->buffer;
                        bufferInfo.range = binding.size;

                        auto& setWrite = setWrites[bindingCounter++];
                        setWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        setWrite.pNext = nullptr;
                        setWrite.dstBinding = binding.layoutBinding.binding;
                        setWrite.dstArrayElement = binding.arrayElement;
                        setWrite.dstSet = descriptorBindingData.sets[i];
                        setWrite.descriptorCount = 1;
                        setWrite.descriptorType = binding.layoutBinding.descriptorType;
                        setWrite.pBufferInfo = &bufferInfo;
                    }

                    // SAMPLED IMAGES
                    for (uint32_t j = 0; j < BINDINGS_PER_DESCRIPTOR_SET; j++) {
                        if (!descriptorBindingData.sampledImages[i][j].first ||
                            !descriptorBindingData.sampledImages[i][j].second) continue;
                        const auto& binding = shader->sets[i].bindings[j];

                        auto [image, sampler] = descriptorBindingData.sampledImages[i][j];

                        auto& imageInfo = imageInfos[bindingCounter];
                        imageInfo.sampler = sampler->sampler;
                        imageInfo.imageView = image->view;
                        imageInfo.imageLayout = image->layout;

                        auto& setWrite = setWrites[bindingCounter++];
                        setWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        setWrite.pNext = nullptr;
                        setWrite.dstBinding = binding.layoutBinding.binding;
                        setWrite.dstArrayElement = binding.arrayElement;
                        setWrite.dstSet = descriptorBindingData.sets[i];
                        setWrite.descriptorCount = 1;
                        setWrite.descriptorType = binding.layoutBinding.descriptorType;
                        setWrite.pImageInfo = &imageInfo;
                    }

                    // STORAGE IMAGES OR IMAGES SEPARATED FROM SAMPLER
                    for (uint32_t j = 0; j < BINDINGS_PER_DESCRIPTOR_SET; j++) {
                        if (!descriptorBindingData.images[i][j]) continue;
                        const auto& binding = shader->sets[i].bindings[j];

                        auto image = descriptorBindingData.images[i][j];

                        auto& imageInfo = imageInfos[bindingCounter];
                        imageInfo.sampler = VK_NULL_HANDLE;
                        imageInfo.imageView = image->view;
                        imageInfo.imageLayout = image->layout;

                        auto& setWrite = setWrites[bindingCounter++];
                        setWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        setWrite.pNext = nullptr;
                        setWrite.dstBinding = binding.layoutBinding.binding;
                        setWrite.dstArrayElement = binding.arrayElement;
                        setWrite.dstSet = descriptorBindingData.sets[i];
                        setWrite.descriptorCount = 1;
                        setWrite.descriptorType = binding.layoutBinding.descriptorType;
                        setWrite.pImageInfo = &imageInfo;
                    }

                    vkUpdateDescriptorSets(device, bindingCounter, setWrites, 0, nullptr);
                }

                if (descriptorBindingData.sets[i] != nullptr) {
                    vkCmdBindDescriptorSets(commandBuffer, pipelineInUse->bindPoint,
                        pipelineInUse->layout, i, 1, &descriptorBindingData.sets[i],
                        dynamicOffsetCounter, dynamicOffsets);
                }

            }

            prevDescriptorBindingData = descriptorBindingData;

        }

        void CommandList::ResetDescriptors() {

            prevDescriptorBindingData.Reset();
            descriptorBindingData.Reset();
            descriptorPool->Reset();

        }

        const VkExtent2D CommandList::GetRenderPassExtent() const {

            VkExtent2D extent = {};

            if (swapChainInUse) {
                extent = swapChainInUse->extent;
            }
            else if (renderPassInUse) {
                extent = renderPassInUse->extent;
            }
            else {
                assert(0 && "No valid render pass found");
            }

            return extent;

        }

    }

}