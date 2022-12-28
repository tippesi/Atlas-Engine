#include "Framebuffer.h"
#include "GraphicsDevice.h"

#include <cassert>

namespace Atlas {

    namespace Graphics {

        FrameBuffer::FrameBuffer(GraphicsDevice *device, FrameBufferDesc &desc) : renderPass(desc.renderPass),
            extent(desc.extent), layers(desc.layers), device(device) {

            for (auto& attachmentDesc : desc.attachments) {

                FrameBufferAttachment attachment {
                  .attachment = attachmentDesc.attachment,
                  .idx = static_cast<uint32_t>(attachmentDesc.attachment),
                  .layer = attachmentDesc.layer,
                  .isValid = true
                };

                if (attachment.attachment == Attachment::DepthAttachment) {
                    depthAttachment = attachment;
                }
                else {
                    colorAttachments[attachment.idx] = attachment;
                }

            }

            Refresh();

            isComplete = true;

        }

        FrameBuffer::~FrameBuffer() {

            vkDestroyFramebuffer(device->device, frameBuffer, nullptr);

        }

        void FrameBuffer::Refresh() {

            std::vector<VkImageView> imageViews;

            const auto& rpColorAttachments = renderPass->colorAttachments;
            const auto& rpDepthAttachment = renderPass->depthAttachment;

            // NOTE: The frame buffer requires to have an image view for each attachment in the render pass
            // This doesn't mean we need to write to all attachments. The writing to attachments is being configured
            // in the pipeline setup and is based on which color attachments the user specified in this frame buffer
            for (uint32_t i = 0; i < MAX_COLOR_ATTACHMENTS; i++) {
                auto& rpColorAttachment = rpColorAttachments[i];
                auto& colorAttachment = colorAttachments[i];
                // No valid render pass attachment
                if (!rpColorAttachment.image) continue;
                // Check if we want to write to this attachment
                if (colorAttachment.isValid) {
                    assert(colorAttachment.layer < rpColorAttachment.image->layers &&
                        "Image doesn't contain this layer");
                    imageViews.push_back(rpColorAttachment.image->layerViews[colorAttachment.layer]);
                }
                else {
                    imageViews.push_back(rpColorAttachment.image->layerViews.front());
                }
            }

            if (rpDepthAttachment.image) {
                if (depthAttachment.isValid) {
                    assert(depthAttachment.layer < rpDepthAttachment.image->layers &&
                        "Image doesn't contain this layer");
                    imageViews.push_back(rpDepthAttachment.image->layerViews[depthAttachment.layer]);
                }
                else {
                    imageViews.push_back(rpDepthAttachment.image->layerViews.front());
                }
            }

            if (isComplete) {
                // Delete the previous frame buffer
                device->memoryManager->DestroyRawAllocation([=]() {
                    vkDestroyFramebuffer(device->device, frameBuffer, nullptr);
                });
            }

            VkFramebufferCreateInfo frameBufferInfo = {};
            frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            frameBufferInfo.renderPass = renderPass->renderPass;
            frameBufferInfo.attachmentCount = uint32_t(imageViews.size());
            frameBufferInfo.pAttachments = imageViews.data();
            frameBufferInfo.width = extent.width;
            frameBufferInfo.height = extent.height;
            frameBufferInfo.layers = layers;
            VK_CHECK(vkCreateFramebuffer(device->device, &frameBufferInfo, nullptr, &frameBuffer))

        }

    }

}