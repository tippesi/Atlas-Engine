#ifndef AE_GRAPHICSRENDERPASS_H
#define AE_GRAPHICSRENDERPASS_H

#include "Common.h"
#include "Image.h"

#include <vector>

namespace Atlas {

    namespace Graphics {

        class GraphicsDevice;

        struct RenderPassAttachment {
            VkFormat imageFormat = {};
            VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            VkImageLayout initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            VkImageLayout outputLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            bool isValid = false;
        };

        struct RenderPassDesc {
            RenderPassAttachment colorAttachments[MAX_COLOR_ATTACHMENTS];
            RenderPassAttachment depthAttachment;

            VkClearValue colorClearValue = { .color = { { 1.0f, 1.0f, 1.0f, 1.0f } } };
            VkClearValue depthClearValue = { .depthStencil = {.depth = 1.0f } };

            bool completeInConstructor = true;
        };

        class RenderPass {

        public:
            RenderPass(GraphicsDevice* device, RenderPassDesc& desc);

            ~RenderPass();

            void AttachColor(RenderPassAttachment& attachment, uint32_t slot);

            void AttachDepth(RenderPassAttachment& attachment);

            void Complete();

            VkRenderPass renderPass = {};

            RenderPassAttachment colorAttachments[MAX_COLOR_ATTACHMENTS];
            RenderPassAttachment depthAttachment;

            uint32_t colorAttachmentCount = 0;

            VkClearValue colorClearValue = { .color = { { 1.0f, 1.0f, 1.0f, 1.0f } } };
            VkClearValue depthClearValue = { .depthStencil = {.depth = 1.0f } };

            bool isComplete = false;
        
        private:
            void CompleteColorAttachment(const RenderPassAttachment& attachment, uint32_t slot);

            void CompleteDepthAttachment(const RenderPassAttachment& attachment);

            std::vector<VkAttachmentDescription2> attachmentDescriptions;
            std::vector<VkSubpassDependency2> subPassDependencies;

            std::vector<VkAttachmentReference2> colorAttachmentReferences;
            VkAttachmentReference2 depthAttachmentReference;

            GraphicsDevice* device;

        };

    }

}

#endif
