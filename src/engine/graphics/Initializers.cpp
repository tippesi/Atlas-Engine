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

            VkPipelineShaderStageCreateInfo InitPipelineShaderStageCreateInfo(VkShaderStageFlagBits stage,
                VkShaderModule shaderModule) {

                VkPipelineShaderStageCreateInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                info.pNext = nullptr;
                info.stage = stage;
                info.module = shaderModule;
                info.pName = "main";

                return info;

            }

            VkPipelineVertexInputStateCreateInfo InitPipelineVertexInputStateCreateInfo(
                VkVertexInputBindingDescription* inputBindingDescriptions, uint32_t inputBindingDescriptionsCount,
                VkVertexInputAttributeDescription* inputAttributeDescriptions, uint32_t inputAttributeDescriptionsCount) {

                VkPipelineVertexInputStateCreateInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
                info.pNext = nullptr;
                info.vertexBindingDescriptionCount = inputBindingDescriptionsCount;
                info.pVertexBindingDescriptions = inputBindingDescriptions;
                info.vertexAttributeDescriptionCount = inputAttributeDescriptionsCount;
                info.pVertexAttributeDescriptions = inputAttributeDescriptions;

                return info;

            }

            VkPipelineInputAssemblyStateCreateInfo InitPipelineInputAssemblyStateCreateInfo(VkPrimitiveTopology topology) {

                VkPipelineInputAssemblyStateCreateInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
                info.pNext = nullptr;
                info.topology = topology;
                info.primitiveRestartEnable = VK_FALSE;

                return info;

            }

            VkPipelineRasterizationStateCreateInfo InitPipelineRasterizationStateCreateInfo(VkPolygonMode polygonMode) {

                VkPipelineRasterizationStateCreateInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
                info.pNext = nullptr;

                info.depthClampEnable = VK_FALSE;
                info.rasterizerDiscardEnable = VK_FALSE;

                info.polygonMode = polygonMode;
                info.lineWidth = 1.0f;
                info.cullMode = VK_CULL_MODE_NONE;
                info.frontFace = VK_FRONT_FACE_CLOCKWISE;
                info.depthBiasEnable = VK_FALSE;
                info.depthBiasConstantFactor = 0.0f;
                info.depthBiasClamp = 0.0f;
                info.depthBiasSlopeFactor = 0.0f;

                return info;

            }

            VkPipelineMultisampleStateCreateInfo InitPipelineMultisampleStateCreateInfo() {

                VkPipelineMultisampleStateCreateInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
                info.pNext = nullptr;
                info.sampleShadingEnable = VK_FALSE;
                info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
                info.minSampleShading = 1.0f;
                info.pSampleMask = nullptr;
                info.alphaToCoverageEnable = VK_FALSE;
                info.alphaToOneEnable = VK_FALSE;

                return info;

            }

            VkPipelineColorBlendAttachmentState InitPipelineColorBlendAttachmentState() {

                VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
                colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                    VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
                colorBlendAttachment.blendEnable = VK_FALSE;

                return colorBlendAttachment;

            }

            VkPipelineLayoutCreateInfo InitPipelineLayoutCreateInfo() {

                VkPipelineLayoutCreateInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
                info.pNext = nullptr;
                info.flags = 0;
                info.setLayoutCount = 0;
                info.pSetLayouts = nullptr;
                info.pushConstantRangeCount = 0;
                info.pPushConstantRanges = nullptr;

                return info;

            }

            VkCommandBufferBeginInfo InitCommandBufferBeginInfo(VkCommandBufferUsageFlags flags) {

                VkCommandBufferBeginInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                info.pNext = nullptr;
                info.pInheritanceInfo = nullptr;
                info.flags = flags;

                return info;

            }

            VkSubmitInfo InitSubmitInfo(VkCommandBuffer* commandBuffer) {

                VkSubmitInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                info.pNext = nullptr;
                info.waitSemaphoreCount = 0;
                info.pWaitSemaphores = nullptr;
                info.pWaitDstStageMask = nullptr;
                info.commandBufferCount = 1;
                info.pCommandBuffers = commandBuffer;
                info.signalSemaphoreCount = 0;
                info.pSignalSemaphores = nullptr;

                return info;

            }

            VkImageCreateInfo InitImageCreateInfo(VkFormat format, VkImageUsageFlags usageFlags,
                VkExtent3D extent, VkImageType imageType) {

                VkImageCreateInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                info.pNext = nullptr;
                info.imageType = imageType;
                info.format = format;
                info.extent = extent;
                info.mipLevels = 1;
                info.arrayLayers = 1;
                info.samples = VK_SAMPLE_COUNT_1_BIT;
                info.tiling = VK_IMAGE_TILING_OPTIMAL;
                info.usage = usageFlags;

                return info;

            }

            VkImageViewCreateInfo InitImageViewCreateInfo(VkFormat format, VkImage image,
                VkImageAspectFlags aspectFlags, VkImageViewType viewType, size_t layerCount) {

                VkImageViewCreateInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                info.pNext = nullptr;
                info.viewType = viewType;
                info.image = image;
                info.format = format;
                info.subresourceRange.baseMipLevel = 0;
                info.subresourceRange.levelCount = 1;
                info.subresourceRange.baseArrayLayer = 0;
                info.subresourceRange.layerCount = uint32_t(layerCount);
                info.subresourceRange.aspectMask = aspectFlags;

                return info;

            }

            VkPipelineDepthStencilStateCreateInfo InitPipelineDepthStencilStateCreateInfo(bool depthTest,
                bool depthWrite, VkCompareOp compareOperation) {

                VkPipelineDepthStencilStateCreateInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
                info.pNext = nullptr;
                info.depthTestEnable = depthTest ? VK_TRUE : VK_FALSE;
                info.depthWriteEnable = depthWrite ? VK_TRUE : VK_FALSE;
                info.depthCompareOp = depthTest ? compareOperation : VK_COMPARE_OP_ALWAYS;
                info.depthBoundsTestEnable = VK_FALSE;
                info.minDepthBounds = 0.0f;
                info.maxDepthBounds = 1.0f;
                info.stencilTestEnable = VK_FALSE;

                return info;

            }

            VkVertexInputBindingDescription InitVertexInputBindingDescription(uint32_t binding, uint32_t stride,
                VkVertexInputRate inputRate) {

                VkVertexInputBindingDescription bindingDescription = {};
                bindingDescription.binding = binding;
                bindingDescription.stride = stride;
                bindingDescription.inputRate = inputRate;

                return bindingDescription;

            }

            VkVertexInputAttributeDescription InitVertexInputAttributeDescription(uint32_t binding, VkFormat format) {

                VkVertexInputAttributeDescription attributeDescription = {};
                attributeDescription.binding = binding;
                attributeDescription.location = binding;
                attributeDescription.format = format;
                attributeDescription.offset = 0;

                return attributeDescription;

            }

            VkSamplerCreateInfo InitSamplerCreateInfo(VkFilter filters, VkSamplerAddressMode samplerAddressMode,
                VkSamplerMipmapMode mipmapMode, float maxLod, float mipLodBias) {

                VkSamplerCreateInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
                info.pNext = nullptr;
                info.magFilter = filters;
                info.minFilter = filters;
                info.addressModeU = samplerAddressMode;
                info.addressModeV = samplerAddressMode;
                info.addressModeW = samplerAddressMode;
                info.mipmapMode = mipmapMode;
                info.maxLod = maxLod;
                info.mipLodBias = mipLodBias;

                return info;

            }

            VkImageMemoryBarrier InitImageMemoryBarrier(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageAspectFlags aspectMask) {

                VkImageMemoryBarrier imageBarrier = {};
                imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                imageBarrier.image = image;
                imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                imageBarrier.subresourceRange.aspectMask = aspectMask;
                imageBarrier.subresourceRange.baseArrayLayer = 0;
                imageBarrier.subresourceRange.layerCount = 1;
                imageBarrier.subresourceRange.levelCount = 1;
                imageBarrier.oldLayout = oldLayout;
                imageBarrier.newLayout = newLayout;
                imageBarrier.srcAccessMask = srcAccessMask;
                imageBarrier.dstAccessMask = dstAccessMask;

                return imageBarrier;

            }

            VkBufferMemoryBarrier InitBufferMemoryBarrier(VkBuffer buffer, VkAccessFlags srcAccessMask,
                VkAccessFlags dstAccessMask) {

                VkBufferMemoryBarrier bufferBarrier = {};
                bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                bufferBarrier.buffer = buffer;
                bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                bufferBarrier.offset = 0;
                bufferBarrier.size = VK_WHOLE_SIZE;
                bufferBarrier.srcAccessMask = srcAccessMask;
                bufferBarrier.dstAccessMask = dstAccessMask;

                return bufferBarrier;

            }

        }

    }

}