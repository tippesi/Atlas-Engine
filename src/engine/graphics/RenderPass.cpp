#include "RenderPass.h"
#include "GraphicsDevice.h"

namespace Atlas {

    namespace Graphics {

        RenderPass::RenderPass(GraphicsDevice* device, RenderPassDesc& desc) : colorClearValue(desc.colorClearValue),
            depthClearValue(desc.depthClearValue), device(device) {

            for (uint32_t i = 0; i < MAX_COLOR_ATTACHMENTS; i++) {
                auto& attachment = desc.colorAttachments[i];
                if (!attachment.image) continue;

                AttachColor(attachment, i);
            }

            if (desc.depthAttachment.image) AttachDepth(desc.depthAttachment);
            if (desc.completeInConstructor) Complete();

        }

        RenderPass::~RenderPass() {

            if (!isComplete) return;

            vkDestroyRenderPass(device->device, renderPass, nullptr);

        }

        void RenderPass::AttachColor(RenderPassAttachment& attachment, uint32_t slot) {

            assert(slot < MAX_COLOR_ATTACHMENTS && "Color attachment slot is not available");

            colorAttachments[slot] = attachment;

        }

        void RenderPass::AttachDepth(RenderPassAttachment& attachment) {

            depthAttachment = attachment;

        }

        void RenderPass::RefreshColorImage(Ref<Image> &image, uint32_t slot) {

            assert(slot < MAX_COLOR_ATTACHMENTS && "Color attachment slot is not available");
            assert(colorAttachments[slot].image && "Attachment wasn't valid");
            assert(image->format == colorAttachments[slot].image->format && "Image formats need to be the same");

            colorAttachments[slot].image = image;

        }

        void RenderPass::RefreshDepthImage(Ref<Image> &image) {

            assert(depthAttachment.image && "Attachment wasn't valid");
            assert(image->format == depthAttachment.image->format && "Image formats need to be the same");

            depthAttachment.image = image;

        }

        Ref<Image> &RenderPass::GetColorImage(uint32_t slot) {

            return colorAttachments[slot].image;

        }

        Ref<Image> &RenderPass::GetDepthImage() {

            return depthAttachment.image;

        }

        void RenderPass::Complete() {

            // Can't create a rende pass twice, might still be in use by device
            if (isComplete) return;

            for (uint32_t i = 0; i < MAX_COLOR_ATTACHMENTS; i++) {
                auto& attachment = colorAttachments[i];
                if (!attachment.image) continue;

                CompleteColorAttachment(attachment, i);
            }

            if (depthAttachment.image) CompleteDepthAttachment(depthAttachment);

            VkSubpassDescription2 subPassDescription = {};
            subPassDescription.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
            subPassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subPassDescription.colorAttachmentCount = uint32_t(colorAttachmentReferences.size());
            subPassDescription.pColorAttachments = colorAttachmentReferences.data();
            if (depthAttachment.image) {
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

        void RenderPass::CompleteColorAttachment(RenderPassAttachment& attachment, uint32_t slot) {

            VkAttachmentDescription2 colorAttachmentDescription = Initializers::InitAttachmentDescription(
                attachment.image->format, attachment.initialLayout, 
                attachment.outputLayout, attachment.loadOp);

            VkSubpassDependency2 colorDependency = {};
            colorDependency.sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
            colorDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            colorDependency.dstSubpass = 0;
            colorDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            colorDependency.srcAccessMask = 0; // TODO: This could be an issue
            colorDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            colorDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            VkAttachmentReference2 colorAttachmentReference = Initializers::InitAttachmentReference(slot,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            attachmentDescriptions.push_back(colorAttachmentDescription);
            subPassDependencies.push_back(colorDependency);

            colorAttachmentReferences.push_back(colorAttachmentReference);

        }

        void RenderPass::CompleteDepthAttachment(RenderPassAttachment& attachment) {

            VkAttachmentDescription2 depthAttachmentDescription = Initializers::InitAttachmentDescription(
                attachment.image->format, attachment.initialLayout,
                attachment.outputLayout, attachment.loadOp);

            VkSubpassDependency2 depthDependency = {};
            depthDependency.sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
            depthDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            depthDependency.dstSubpass = 0;
            depthDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            depthDependency.srcAccessMask = 0;
            depthDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            depthDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            uint32_t depthAttachmentSlot = uint32_t(colorAttachmentReferences.size());
            VkAttachmentReference2 depthAttachmentReference = Initializers::InitAttachmentReference(depthAttachmentSlot,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

            attachmentDescriptions.push_back(depthAttachmentDescription);
            subPassDependencies.push_back(depthDependency);

            this->depthAttachmentReference = depthAttachmentReference;

        }

    }

}