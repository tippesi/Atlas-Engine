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

            VkPipelineShaderStageCreateInfo InitPipelineShaderStageCreateInfo(VkShaderStageFlagBits stage,
                VkShaderModule shaderModule);

            VkPipelineVertexInputStateCreateInfo InitPipelineVertexInputStateCreateInfo(
                VkVertexInputBindingDescription* inputBindingDescriptions = nullptr,
                uint32_t inputBindingDescriptionsCount = 0,
                VkVertexInputAttributeDescription* inputAttributeDescriptions = nullptr,
                uint32_t inputAttributeDescriptionsCount = 0
                );

            VkPipelineInputAssemblyStateCreateInfo InitPipelineInputAssemblyStateCreateInfo(VkPrimitiveTopology topology);

            VkPipelineRasterizationStateCreateInfo InitPipelineRasterizationStateCreateInfo(VkPolygonMode polygonMode);

            VkPipelineMultisampleStateCreateInfo InitPipelineMultisampleStateCreateInfo();

            VkPipelineColorBlendAttachmentState InitPipelineColorBlendAttachmentState();

            VkPipelineLayoutCreateInfo InitPipelineLayoutCreateInfo();

            VkCommandBufferBeginInfo InitCommandBufferBeginInfo(VkCommandBufferUsageFlags flags = 0);

            VkSubmitInfo InitSubmitInfo(VkCommandBuffer* commandBuffer);

            VkImageCreateInfo InitImageCreateInfo(VkFormat format, VkImageUsageFlags usageFlags,
                VkExtent3D extent, VkImageType imageType = VK_IMAGE_TYPE_2D);

            VkImageViewCreateInfo InitImageViewCreateInfo(VkFormat format, VkImage image,
                VkImageAspectFlags aspectFlags, VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D,
                size_t layerCount = 1);

            VkPipelineDepthStencilStateCreateInfo InitPipelineDepthStencilStateCreateInfo(bool depthTest,
                bool depthWrite, VkCompareOp compareOperation);

            VkVertexInputBindingDescription InitVertexInputBindingDescription(uint32_t binding, uint32_t stride,
                VkVertexInputRate inputRate = VK_VERTEX_INPUT_RATE_VERTEX);

            VkVertexInputAttributeDescription InitVertexInputAttributeDescription(uint32_t binding, VkFormat format);

            VkSamplerCreateInfo InitSamplerCreateInfo(VkFilter filters, VkSamplerAddressMode samplerAddressMode,
                VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST, float maxLod = 0.0f,
                float mipLodBias = 0.0f);

            VkImageMemoryBarrier InitImageMemoryBarrier(VkImage image, VkImageLayout oldLayout,
                VkImageLayout newLayout, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
                VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);

            VkBufferMemoryBarrier InitBufferMemoryBarrier(VkBuffer buffer, VkAccessFlags srcAccessMask,
                VkAccessFlags dstAccessMask);

        }

    }

}

#endif
