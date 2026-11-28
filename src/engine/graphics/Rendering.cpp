#include "Rendering.h"

namespace Atlas {

    namespace Graphics {

        VkExtent2D RenderingDesc::GetExtent() const {

            const Image* image = depthAttachment.image.get();
            for (const auto& attachment : colorAttachments) {
                if (attachment.image) {
                    image = attachment.image.get();
                    break;
                }
            }

            AE_ASSERT(image && "A render pass needs at least one attachment");
            return image ? VkExtent2D{ image->width, image->height } : VkExtent2D{};

        }

        AttachmentFormats::AttachmentFormats(const RenderingDesc& desc) {

            for (uint32_t i = 0; i < MAX_COLOR_ATTACHMENTS; i++) {
                const auto& attachment = desc.colorAttachments[i];
                if (!attachment.image) continue;

                colorFormats[i] = attachment.image->format;
                colorWriteEnabled[i] = attachment.writeEnabled;
                colorAttachmentCount = i + 1;
            }

            if (desc.depthAttachment.image)
                depthFormat = desc.depthAttachment.image->format;

        }

    }

}
