#include "RenderPass.h"
#include "GraphicsDevice.h"

namespace Atlas {

    namespace Graphics {

        RenderPass::RenderPass(GraphicsDevice* device, const RenderPassDesc& desc) :
            colorClearValue(desc.colorClearValue), depthClearValue(desc.depthClearValue), device(device) {

            for (uint32_t i = 0; i < MAX_COLOR_ATTACHMENTS; i++) {
                auto& attachment = desc.colorAttachments[i];
                if (!attachment.imageFormat) continue;

                AttachColor(attachment, i);
            }

            if (desc.depthAttachment.imageFormat) AttachDepth(desc.depthAttachment);
            if (desc.completeInConstructor) Complete();

        }

        RenderPass::~RenderPass() {

            if (!isComplete) return;

            vkDestroyRenderPass(device->device, renderPass, nullptr);

        }

        void RenderPass::AttachColor(const RenderPassColorAttachment& attachment, uint32_t slot) {

            AE_ASSERT(slot < MAX_COLOR_ATTACHMENTS && "Color attachment slot is not available");

            colorAttachments[slot] = attachment;
            colorAttachments[slot].isValid = true;

        }

        void RenderPass::AttachDepth(const RenderPassDepthAttachment& attachment) {

            depthAttachment = attachment;
            depthAttachment.isValid = true;

        }

        void RenderPass::Complete() {

            // Can't create a rende pass twice, might still be in use by device
            if (isComplete) return;

            for (uint32_t i = 0; i < MAX_COLOR_ATTACHMENTS; i++) {
                auto& attachment = colorAttachments[i];
                if (!attachment.imageFormat) continue;

                CompleteColorAttachment(attachment, i);
            }

            if (depthAttachment.imageFormat) CompleteDepthAttachment(depthAttachment);

            VkSubpassDescription2 subPassDescription = {};
            subPassDescription.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
            subPassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subPassDescription.colorAttachmentCount = uint32_t(colorAttachmentReferences.size());
            subPassDescription.pColorAttachments = colorAttachmentReferences.data();
            if (depthAttachment.imageFormat) {
                subPassDescription.pDepthStencilAttachment = &depthAttachmentReference;
            }

            colorAttachmentCount = subPassDescription.colorAttachmentCount;

            VkRenderPassCreateInfo2 renderPassCreateInfo = Initializers::InitRenderPassCreateInfo(
                uint32_t(attachmentDescriptions.size()), attachmentDescriptions.data(), 1, &subPassDescription);

            renderPassCreateInfo.dependencyCount = uint32_t(subPassDependencies.size());
            renderPassCreateInfo.pDependencies = subPassDependencies.data();

            VK_CHECK(vkCreateRenderPass2(device->device, &renderPassCreateInfo, nullptr, &renderPass))

            isComplete = true;

        }

        void RenderPass::CompleteColorAttachment(const RenderPassColorAttachment& attachment, uint32_t slot) {

            VkAttachmentDescription2 colorAttachmentDescription = Initializers::InitAttachmentDescription(
                attachment.imageFormat, attachment.initialLayout,
                attachment.outputLayout, attachment.loadOp);

            VkSubpassDependency2 colorDependency = {};
            colorDependency.sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
            colorDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            colorDependency.dstSubpass = 0;
            colorDependency.srcStageMask = attachment.srcStageMask;
            colorDependency.srcAccessMask = attachment.srcAccessMask;
            colorDependency.dstStageMask = attachment.dstStageMask;
            colorDependency.dstAccessMask = attachment.dstAccessMask;

            VkAttachmentReference2 colorAttachmentReference = Initializers::InitAttachmentReference(slot,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            attachmentDescriptions.push_back(colorAttachmentDescription);
            subPassDependencies.push_back(colorDependency);

            colorAttachmentReferences.push_back(colorAttachmentReference);

        }

        void RenderPass::CompleteDepthAttachment(const RenderPassDepthAttachment& attachment) {

            VkAttachmentDescription2 depthAttachmentDescription = Initializers::InitAttachmentDescription(
                attachment.imageFormat, attachment.initialLayout,
                attachment.outputLayout, attachment.loadOp);

            VkSubpassDependency2 depthDependency = {};
            depthDependency.sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
            depthDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            depthDependency.dstSubpass = 0;
            depthDependency.srcStageMask = attachment.srcStageMask;
            depthDependency.srcAccessMask = attachment.srcAccessMask;
            depthDependency.dstStageMask = attachment.dstStageMask;
            depthDependency.dstAccessMask = attachment.dstAccessMask;

            uint32_t depthAttachmentSlot = uint32_t(colorAttachmentReferences.size());
            depthAttachmentReference = Initializers::InitAttachmentReference(depthAttachmentSlot,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

            attachmentDescriptions.push_back(depthAttachmentDescription);
            subPassDependencies.push_back(depthDependency);

        }

    }

}