#include "CommandList.h"
#include "GraphicsDevice.h"

#include <cassert>

namespace Atlas {

    namespace Graphics {

        CommandList::CommandList(GraphicsDevice* device, QueueType queueType, uint32_t queueFamilyIndex) :
            device(device->device), queueType(queueType), queueFamilyIndex(queueFamilyIndex) {

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

        void CommandList::BeginRenderPass(Ref<RenderPass>& renderPass, bool clear) {

            VkRenderPassBeginInfo rpInfo = {};
            rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpInfo.pNext = nullptr;
            rpInfo.renderPass = renderPass->renderPass;
            rpInfo.renderArea.offset.x = 0;
            rpInfo.renderArea.offset.y = 0;
            rpInfo.renderArea.extent = renderPass->extent;
            rpInfo.framebuffer = renderPass->frameBuffer;

            vkCmdBeginRenderPass(commandBuffer, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
            renderPassInUse = renderPass;

            if (clear) ClearAttachments();

        }

        void CommandList::EndRenderPass() {

            vkCmdEndRenderPass(commandBuffer);

            // We need to keep track of the image layouts
            if (swapChainInUse) {

            }
            if (renderPassInUse) {

            }

            swapChainInUse = nullptr;
            renderPassInUse = nullptr;

        }

        void CommandList::BindPipeline(Ref<Pipeline>& pipeline) {

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
                for (uint32_t i = 0; i < MAX_COLOR_ATTACHMENTS; i++) {
                    if (!renderPassInUse->colorAttachments[i].image) continue;
                    colorClear.clearValue = renderPassInUse->colorClearValue;
                    colorClear.colorAttachment = i;
                    clearAttachments.push_back(colorClear);
                }

                depthClear.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                depthClear.clearValue = renderPassInUse->depthClearValue;
                depthClear.colorAttachment = 0;
                clearAttachments.push_back(depthClear);

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

        void CommandList::BindBuffer(Ref<Buffer>& buffer, uint32_t set, uint32_t binding) {

            assert(set < DESCRIPTOR_SET_COUNT && "Descriptor set not allowed for use");
            assert(binding < BINDINGS_PER_DESCRIPTOR_SET && "The binding point not allowed for use");

            // Since the buffer is partially owned by the device, we can safely get the pointer for this frame
            descriptorBindingData.buffers[set][binding] = buffer.get();

        }

        void CommandList::BindBuffer(Ref<MultiBuffer>& buffer, uint32_t set, uint32_t binding) {

            assert(set < DESCRIPTOR_SET_COUNT && "Descriptor set not allowed for use");
            assert(binding < BINDINGS_PER_DESCRIPTOR_SET && "The binding point not allowed for use");

            // Since the buffer is partially owned by the device, we can safely get the pointer for this frame
            descriptorBindingData.buffers[set][binding] = buffer->GetCurrent();

        }

        void CommandList::BindImage(Ref<Image>& image, uint32_t set, uint32_t binding) {

            assert(set < DESCRIPTOR_SET_COUNT && "Descriptor set not allowed for use");
            assert(binding < BINDINGS_PER_DESCRIPTOR_SET && "The binding point not allowed for use");

            descriptorBindingData.images[set][binding] = image.get();

        }

        void CommandList::BindImage(Ref<Image>& image, Ref<Sampler>& sampler, uint32_t set, uint32_t binding) {

            assert(set < DESCRIPTOR_SET_COUNT && "Descriptor set not allowed for use");
            assert(binding < BINDINGS_PER_DESCRIPTOR_SET && "The binding point not allowed for use");

            descriptorBindingData.sampledImages[set][binding] = { image.get(), sampler.get() };

        }

        void CommandList::ResetBindings() {

            descriptorBindingData.Reset();

        }

        void CommandList::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex,
            int32_t vertexOffset, uint32_t firstInstance) {

            assert(pipelineInUse && "No pipeline is bound");
            if (!pipelineInUse) return;
            assert(indexCount && instanceCount && "Index or instance count should not be zero");

            BindDescriptorSets();

            vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);

        }

        void CommandList::BindDescriptorSets() {

            VkWriteDescriptorSet setWrites[2 * BINDINGS_PER_DESCRIPTOR_SET];
            VkDescriptorBufferInfo bufferInfos[2 * BINDINGS_PER_DESCRIPTOR_SET];
            VkDescriptorImageInfo imageInfos[2 * BINDINGS_PER_DESCRIPTOR_SET];

            auto shader = pipelineInUse->shader;

            for (uint32_t i = 0; i < DESCRIPTOR_SET_COUNT; i++) {

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

                    vkUpdateDescriptorSets(device, bindingCounter, setWrites, 0, nullptr);
                }

                if (descriptorBindingData.sets[i] != nullptr) {
                    uint32_t offset = 0;
                    vkCmdBindDescriptorSets(commandBuffer, pipelineInUse->bindPoint,
                        pipelineInUse->layout, i, 1, &descriptorBindingData.sets[i], 1, &offset);
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