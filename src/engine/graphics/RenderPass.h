#pragma once

#include "Common.h"
#include "Image.h"

namespace Atlas {

    namespace Graphics {

        class GraphicsDevice;

        struct RenderPassColorAttachment {
            VkFormat imageFormat = {};
            VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            VkImageLayout initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            VkImageLayout outputLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            VkAccessFlags srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            VkAccessFlags dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            bool isValid = false;
        };

        struct RenderPassDepthAttachment {
            VkFormat imageFormat = {};
            VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            VkImageLayout initialLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            VkImageLayout outputLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            VkAccessFlags srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            VkAccessFlags dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            bool isValid = false;
        };

        struct RenderPassDesc {
            RenderPassColorAttachment colorAttachments[MAX_COLOR_ATTACHMENTS];
            RenderPassDepthAttachment depthAttachment;

            VkClearValue colorClearValue = { .color = { { 0.0f, 0.0f, 0.0f, 1.0f } } };
            VkClearValue depthClearValue = { .depthStencil = {.depth = 1.0f } };
        };

        // Describes the attachments of a pass, which is recorded with dynamic rendering
        class RenderPass {

        public:
            RenderPass(GraphicsDevice* device, const RenderPassDesc& desc);

            RenderPassColorAttachment colorAttachments[MAX_COLOR_ATTACHMENTS];
            RenderPassDepthAttachment depthAttachment;

            // Number of color attachment slots up to the last valid one
            uint32_t colorAttachmentCount = 0;

            VkClearValue colorClearValue = { .color = { { 1.0f, 1.0f, 1.0f, 1.0f } } };
            VkClearValue depthClearValue = { .depthStencil = {.depth = 1.0f } };

        };

    }

}