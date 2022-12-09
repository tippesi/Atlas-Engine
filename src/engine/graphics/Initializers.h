#ifndef AE_GRAPHICSINITIALIZERS_H
#define AE_GRAPHICSINITIALIZERS_H

#include <volk.h>

namespace Atlas {

    namespace Graphics {

        namespace Initializers {

            VkCommandPoolCreateInfo InitCommandPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);

            VkCommandBufferAllocateInfo InitCommandBufferAllocateInfo(VkCommandPool commandPool, uint32_t bufferCount,
                VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

            VkAttachmentDescription2 InitAttachmentDescription(VkFormat format, VkImageLayout initialLayout,
                VkImageLayout finalLayout, VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE,VkAttachmentDescriptionFlags flags = 0);

            VkAttachmentReference2 InitAttachmentReference(uint32_t attachment, VkImageLayout layout);

            VkRenderPassCreateInfo2 InitRenderPassCreateInfo(uint32_t attachmentCount, VkAttachmentDescription2* attachmentDescription,
                uint32_t subPassCount, VkSubpassDescription2* subPassDescription, VkRenderPassCreateFlags flags = 0);

            VkFramebufferCreateInfo InitFramebufferCreateInfo(VkRenderPass renderPass, uint32_t attachmentCount,
                uint32_t width, uint32_t height, uint32_t layers = 1);

            VkFenceCreateInfo InitFenceCreateInfo(VkFenceCreateFlags flags = 0);

            VkSemaphoreCreateInfo InitSemaphoreCreateInfo(VkSemaphoreCreateFlags flags = 0);

        }

    }

}

#endif
