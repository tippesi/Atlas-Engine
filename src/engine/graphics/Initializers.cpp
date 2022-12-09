#include "Initializers.h"

namespace Atlas {

    namespace Graphics {

        namespace Initializers {

            VkCommandPoolCreateInfo InitCommandPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags) {

                VkCommandPoolCreateInfo commandPoolInfo = {};
                commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                commandPoolInfo.pNext = nullptr;
                commandPoolInfo.queueFamilyIndex = queueFamilyIndex;
                commandPoolInfo.flags = flags;

                return commandPoolInfo;

            }

            VkCommandBufferAllocateInfo InitCommandBufferAllocateInfo(VkCommandPool commandPool, uint32_t bufferCount,
                VkCommandBufferLevel level) {

                VkCommandBufferAllocateInfo cmdAllocInfo = {};
                cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                cmdAllocInfo.pNext = nullptr;
                cmdAllocInfo.commandPool = commandPool;
                cmdAllocInfo.commandBufferCount = bufferCount;
                cmdAllocInfo.level = level;

                return cmdAllocInfo;

            }

            VkAttachmentDescription2 InitAttachmentDescription(VkFormat format,  VkImageLayout initialLayout,
                VkImageLayout finalLayout, VkAttachmentLoadOp loadOp,
                VkAttachmentStoreOp storeOp,VkAttachmentDescriptionFlags) {

                VkAttachmentDescription2 attachmentDescription = {};
                attachmentDescription.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
                attachmentDescription.format = format;
                attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
                attachmentDescription.loadOp = loadOp;
                attachmentDescription.storeOp = storeOp;
                attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                attachmentDescription.initialLayout = initialLayout;
                attachmentDescription.finalLayout = finalLayout;

                return attachmentDescription;

            }

            VkAttachmentReference2 InitAttachmentReference(uint32_t attachment, VkImageLayout layout) {

                VkAttachmentReference2 attachmentReference = {};
                attachmentReference.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
                attachmentReference.attachment = attachment;
                attachmentReference.layout = layout;

                return attachmentReference;

            }

            VkRenderPassCreateInfo2 InitRenderPassCreateInfo(uint32_t attachmentCount, VkAttachmentDescription2* attachmentDescription,
                uint32_t subPassCount, VkSubpassDescription2* subPassDescription, VkRenderPassCreateFlags flags) {

                VkRenderPassCreateInfo2 renderPassInfo = {};
                renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2;
                renderPassInfo.attachmentCount = attachmentCount;
                renderPassInfo.pAttachments = attachmentDescription;
                renderPassInfo.subpassCount = subPassCount;
                renderPassInfo.pSubpasses = subPassDescription;

                return renderPassInfo;

            }

            VkFramebufferCreateInfo InitFramebufferCreateInfo(VkRenderPass renderPass, uint32_t attachmentCount,
                uint32_t width, uint32_t height, uint32_t layers) {

                VkFramebufferCreateInfo frameBufferInfo = {};
                frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                frameBufferInfo.renderPass = renderPass;
                frameBufferInfo.attachmentCount = 1;
                frameBufferInfo.width = width;
                frameBufferInfo.height = height;
                frameBufferInfo.layers = layers;

                return frameBufferInfo;

            }

            VkFenceCreateInfo InitFenceCreateInfo(VkFenceCreateFlags flags) {

                VkFenceCreateInfo fenceCreateInfo = {};
                fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                fenceCreateInfo.pNext = nullptr;
                fenceCreateInfo.flags = flags;

                return fenceCreateInfo;

            }

            VkSemaphoreCreateInfo InitSemaphoreCreateInfo(VkSemaphoreCreateFlags flags) {

                VkSemaphoreCreateInfo semaphoreCreateInfo = {};
                semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
                semaphoreCreateInfo.pNext = nullptr;
                semaphoreCreateInfo.flags = flags;

                return semaphoreCreateInfo;

            }

        }

    }

}