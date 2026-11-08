#include "Framebuffer.h"

#include <cassert>

namespace Atlas {

    namespace Graphics {

        FrameBuffer::FrameBuffer(GraphicsDevice *device, const FrameBufferDesc &desc) : renderPass(desc.renderPass),
            extent(desc.extent) {

            for (uint32_t i = 0; i < MAX_COLOR_ATTACHMENTS; i++) {
                auto& attachmentDesc = desc.colorAttachments[i];

                FrameBufferAttachment attachment {
                  .image = attachmentDesc.image,
                  .layer = attachmentDesc.layer,
                  .isValid = attachmentDesc.image != nullptr,
                  .writeEnabled = attachmentDesc.writeEnabled
                };

                colorAttachments[i] = attachment;

                // Pipelines take the attachment formats of the render pass, the images have to match them
                const auto& rpColorAttachment = renderPass->colorAttachments[i];
                AE_ASSERT(rpColorAttachment.isValid == attachment.isValid && "Color attachment doesn't match the render pass");
                AE_ASSERT((!attachment.isValid || rpColorAttachment.imageFormat == attachment.image->format) &&
                    "Image format doesn't match the format of the attachment in the render pass");
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

            AE_ASSERT(renderPass->depthAttachment.isValid == depthAttachment.isValid && "Depth attachment doesn't match the render pass");

        }

        void FrameBuffer::ChangeColorAttachmentImage(const Ref<Image> &image, const uint32_t slot) {

            AE_ASSERT(slot < MAX_COLOR_ATTACHMENTS && "Color attachment slot is not available");
            AE_ASSERT(colorAttachments[slot].isValid && "Color attachment is not valid");
            AE_ASSERT(image->format == renderPass->colorAttachments[slot].imageFormat &&
                "Image format doesn't match the format of the attachment in the render pass");

            colorAttachments[slot].image = image;

        }

        void FrameBuffer::ChangeColorAttachmentImage(const Ref<Image>& image, const uint32_t layer, const uint32_t slot) {

            AE_ASSERT(slot < MAX_COLOR_ATTACHMENTS && "Color attachment slot is not available");
            AE_ASSERT(colorAttachments[slot].isValid && "Color attachment is not valid");
            AE_ASSERT(image->format == renderPass->colorAttachments[slot].imageFormat &&
                "Image format doesn't match the format of the attachment in the render pass");

            colorAttachments[slot].image = image;
            colorAttachments[slot].layer = layer;

        }

        void FrameBuffer::ChangeDepthAttachmentImage(const Ref<Image> &image) {

            AE_ASSERT(depthAttachment.isValid && "Depth attachment is not valid");
            AE_ASSERT(image->format == renderPass->depthAttachment.imageFormat &&
                "Image format doesn't match the format of the attachment in the render pass");

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