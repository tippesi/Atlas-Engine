#include "RenderPass.h"
#include "GraphicsDevice.h"

namespace Atlas {

    namespace Graphics {

        RenderPass::RenderPass(GraphicsDevice* device, const RenderPassDesc& desc) :
            colorClearValue(desc.colorClearValue), depthClearValue(desc.depthClearValue) {

            for (uint32_t i = 0; i < MAX_COLOR_ATTACHMENTS; i++) {
                auto& attachment = desc.colorAttachments[i];
                if (!attachment.imageFormat) continue;

                colorAttachments[i] = attachment;
                colorAttachments[i].isValid = true;
                colorAttachmentCount = i + 1;
            }

            if (desc.depthAttachment.imageFormat) {
                depthAttachment = desc.depthAttachment;
                depthAttachment.isValid = true;
            }

        }

    }

}