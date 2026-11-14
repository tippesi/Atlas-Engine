#pragma once

#include "Common.h"
#include "Image.h"

namespace Atlas {

    namespace Graphics {

        struct RenderingAttachment {
            Ref<Image> image = nullptr;
            uint32_t layer = 0;

            VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            VkImageLayout outputLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            // Pipelines of the pass don't write into the attachment if disabled
            bool writeEnabled = true;
        };

        // The attachments of a pass, which is recorded with dynamic rendering
        struct RenderingDesc {
            RenderingAttachment colorAttachments[MAX_COLOR_ATTACHMENTS];
            RenderingAttachment depthAttachment;

            VkClearValue colorClearValue = { .color = { { 0.0f, 0.0f, 0.0f, 1.0f } } };
            VkClearValue depthClearValue = { .depthStencil = {.depth = 1.0f } };

            // All attachments have the same size
            VkExtent2D GetExtent() const;

            // Masks of the barriers around a pass
            static constexpr VkPipelineStageFlags colorStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            static constexpr VkAccessFlags colorAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            static constexpr VkPipelineStageFlags depthStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            static constexpr VkAccessFlags depthAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        };

        // The attachments of a pass as far as its pipelines are concerned. The write enables aren't part
        // of the pipeline variant hash, passes with the same formats need the same write enables
        struct AttachmentFormats {
            AttachmentFormats() = default;

            explicit AttachmentFormats(const RenderingDesc& desc);

            VkFormat colorFormats[MAX_COLOR_ATTACHMENTS] = {};
            bool colorWriteEnabled[MAX_COLOR_ATTACHMENTS] = {};
            VkFormat depthFormat = VK_FORMAT_UNDEFINED;

            // Number of color attachment slots up to the last valid one
            uint32_t colorAttachmentCount = 0;
        };

    }

}
