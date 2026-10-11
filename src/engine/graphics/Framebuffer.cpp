#include "Framebuffer.h"
#include "GraphicsDevice.h"

#include <cassert>

namespace Atlas {

    namespace Graphics {

        FrameBuffer::FrameBuffer(GraphicsDevice *device, const FrameBufferDesc &desc) : renderPass(desc.renderPass),
            extent(desc.extent), layers(desc.layers), device(device) {

            for (uint32_t i = 0; i < MAX_COLOR_ATTACHMENTS; i++) {
                auto& attachmentDesc = desc.colorAttachments[i];

                FrameBufferAttachment attachment {
                  .image = attachmentDesc.image,
                  .layer = attachmentDesc.layer,
                  .isValid = attachmentDesc.image != nullptr,
                  .writeEnabled = attachmentDesc.writeEnabled
                };

                colorAttachments[i] = attachment;
            }

            if (desc.depthAttachment.image) {
                auto& attachmentDesc = desc.depthAttachment;

                FrameBufferAttachment attachment {
                    .image = attachmentDesc.image,
                    .layer = attachmentDesc.layer,
                    .isValid = attachmentDesc.image != nullptr,
                    .writeEnabled = attachmentDesc.writeEnabled
                };

                depthAttachment = attachment;
            }

            Refresh();

            isComplete = true;

        }

        FrameBuffer::~FrameBuffer() {

            vkDestroyFramebuffer(device->device, frameBuffer, nullptr);

        }

        void FrameBuffer::Refresh() {

            const auto& rpColorAttachments = renderPass->colorAttachments;
            const auto& rpDepthAttachment = renderPass->depthAttachment;

            imageViews.reserve(MAX_COLOR_ATTACHMENTS + 1);
            imageViews.clear();

            // NOTE: The frame buffer requires to have an image view for each attachment in the render pass
            // This doesn't mean we need to write to all attachments. The writing to attachments is being configured
            // in the pipeline setup and is based on which color attachments the user specified in this frame buffer
            for (uint32_t i = 0; i < MAX_COLOR_ATTACHMENTS; i++) {
                auto& rpColorAttachment = rpColorAttachments[i];
                auto& colorAttachment = colorAttachments[i];

                AE_ASSERT(rpColorAttachment.isValid == colorAttachment.isValid && "Framebuffer color attachment \
                    to render pass color attachment mismatch");
                if (!rpColorAttachment.isValid) continue;

                AE_ASSERT(rpColorAttachment.imageFormat == colorAttachment.image->format && "Image format doesn't \
                    match the format of the attachment in the render pass");
                // Check if we want to write to this attachment
                if (colorAttachment.isValid) {
                    AE_ASSERT(colorAttachment.layer < colorAttachment.image->layers &&
                        "Image doesn't contain this layer");
                    imageViews.push_back(colorAttachment.image->attachmentViews[colorAttachment.layer]);
                }
            }

            AE_ASSERT(rpDepthAttachment.isValid == depthAttachment.isValid && "Framebuffer depth attachment \
                    to render pass depth attachment mismatch");
            if (depthAttachment.isValid) {
                AE_ASSERT(depthAttachment.layer < depthAttachment.image->layers &&
                       "Image doesn't contain this layer");
                imageViews.push_back(depthAttachment.image->attachmentViews[depthAttachment.layer]);
            }

            if (isComplete) {
                // Delete the previous frame buffer
                device->memoryManager->DestroyRawAllocation(
                    // We need to have copies of the captured objects since
                    // frame buffer will be used again to create a new one and
                    // the frame buffer object might be destroyed in the meanwhile
                    [device = device, frameBuffer = frameBuffer]() {
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

        void FrameBuffer::ChangeColorAttachmentImage(const Ref<Image> &image, const uint32_t slot) {

            AE_ASSERT(slot < MAX_COLOR_ATTACHMENTS && "Color attachment slot is not available");
            AE_ASSERT(colorAttachments[slot].isValid && "Color attachment is not valid");

            colorAttachments[slot].image = image;

        }

        void FrameBuffer::ChangeColorAttachmentImage(const Ref<Image>& image, const uint32_t layer, const uint32_t slot) {

            AE_ASSERT(slot < MAX_COLOR_ATTACHMENTS && "Color attachment slot is not available");
            AE_ASSERT(colorAttachments[slot].isValid && "Color attachment is not valid");

            colorAttachments[slot].image = image;
            colorAttachments[slot].layer = layer;

        }

        void FrameBuffer::ChangeDepthAttachmentImage(const Ref<Image> &image) {

            AE_ASSERT(depthAttachment.isValid && "Depth attachment is not valid");

            depthAttachment.image = image;

        }

        Ref<Image> &FrameBuffer::GetColorImage(uint32_t slot) {

            AE_ASSERT(slot < MAX_COLOR_ATTACHMENTS && "Color attachment slot is not available");

            return colorAttachments[slot].image;

        }

        Ref<Image> &FrameBuffer::GetDepthImage() {

            return depthAttachment.image;

        }

    }

}