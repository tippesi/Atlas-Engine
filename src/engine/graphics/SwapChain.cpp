#include "SwapChain.h"
#include "GraphicsDevice.h"

#include <cstdint>
#include <limits>
#include <algorithm>
#include <cassert>

namespace Atlas {

    namespace Graphics {

        SwapChainSupportDetails::SwapChainSupportDetails(VkPhysicalDevice device, VkSurfaceKHR surface) {

            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &capabilities);

            uint32_t formatCount;
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
            if (formatCount != 0) {
                formats.resize(formatCount);
                vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, formats.data());
            }

            uint32_t presentModeCount;
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
            if (presentModeCount != 0) {
                presentModes.resize(presentModeCount);
                vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, presentModes.data());
            }

        }

        bool SwapChainSupportDetails::IsAdequate() const {

            return formats.size() && presentModes.size();

        }

        SwapChain::SwapChain(const SwapChainSupportDetails& supportDetails, VkSurfaceKHR surface,
            GraphicsDevice* device, int desiredWidth, int32_t desiredHeight, bool preferHDR,
            VkPresentModeKHR desiredMode, SwapChain* oldSwapchain) : device(device) {

            surfaceFormat = ChooseSurfaceFormat(supportDetails.formats, preferHDR);
            presentMode = ChoosePresentMode(supportDetails.presentModes, desiredMode);
            extent = ChooseExtent(supportDetails.capabilities, desiredWidth, desiredHeight);

            if (extent.width == 0 || extent.height == 0) {
                return;
            }

            uint32_t imageCount = supportDetails.capabilities.minImageCount + 1;
            if (supportDetails.capabilities.maxImageCount > 0 &&
                supportDetails.capabilities.maxImageCount < imageCount) {
                imageCount = supportDetails.capabilities.maxImageCount;
            }

            VkSwapchainCreateInfoKHR createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            createInfo.surface = surface;
            createInfo.minImageCount = imageCount;
            createInfo.imageFormat = surfaceFormat.format;
            createInfo.imageColorSpace = surfaceFormat.colorSpace;
            createInfo.imageExtent = extent;
            createInfo.imageArrayLayers = 1;
            createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            createInfo.preTransform = supportDetails.capabilities.currentTransform;
            createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            createInfo.presentMode = presentMode;
            createInfo.clipped = VK_TRUE;
            createInfo.oldSwapchain = oldSwapchain ? oldSwapchain->swapChain : VK_NULL_HANDLE;

            VK_CHECK(vkCreateSwapchainKHR(device->device, &createInfo, nullptr, &swapChain))

            VK_CHECK(vkGetSwapchainImagesKHR(device->device, swapChain, &imageCount, nullptr))
            images.resize(imageCount);
            VK_CHECK(vkGetSwapchainImagesKHR(device->device, swapChain, &imageCount, images.data()))

            VkExtent3D depthImageExtent = { extent.width, extent.height, 1} ;

            const VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
            VkImageCreateInfo imageCreateInfo = Initializers::InitImageCreateInfo(depthFormat,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImageExtent);

            VmaAllocationCreateInfo allocationCreateInfo = {};
            allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            allocationCreateInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            vmaCreateImage(device->memoryManager->allocator, &imageCreateInfo, &allocationCreateInfo,
                &depthImageAllocation.image, &depthImageAllocation.allocation, nullptr);

            VkImageViewCreateInfo depthImageViewCreateInfo = Initializers::InitImageViewCreateInfo(depthFormat,
                depthImageAllocation.image, VK_IMAGE_ASPECT_DEPTH_BIT);
            VK_CHECK(vkCreateImageView(device->device, &depthImageViewCreateInfo, nullptr, &depthImageView));

            VkAttachmentDescription2 colorAttachmentDescription = Initializers::InitAttachmentDescription(
                surfaceFormat.format, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_ATTACHMENT_LOAD_OP_LOAD);
            VkAttachmentDescription2 depthAttachmentDescription = Initializers::InitAttachmentDescription(
                depthFormat, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                VK_ATTACHMENT_LOAD_OP_LOAD);
            VkAttachmentReference2 colorAttachmentReference = Initializers::InitAttachmentReference(0,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            VkAttachmentReference2 depthAttachmentReference = Initializers::InitAttachmentReference(1,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

            VkSubpassDescription2 subPassDescription = {};
            subPassDescription.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
            subPassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subPassDescription.colorAttachmentCount = 1;
            subPassDescription.pColorAttachments = &colorAttachmentReference;
            subPassDescription.pDepthStencilAttachment = &depthAttachmentReference;

            VkAttachmentDescription2 attachmentDescriptions[] =
                { colorAttachmentDescription, depthAttachmentDescription };
            VkRenderPassCreateInfo2 renderPassCreateInfo = Initializers::InitRenderPassCreateInfo(2,
                attachmentDescriptions, 1, &subPassDescription);

            VkSubpassDependency2 colorDependency = {};
            colorDependency.sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
            colorDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            colorDependency.dstSubpass = 0;
            colorDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            colorDependency.srcAccessMask = 0;
            colorDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            colorDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            VkSubpassDependency2 depthDependency = {};
            depthDependency.sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
            depthDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            depthDependency.dstSubpass = 0;
            depthDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            depthDependency.srcAccessMask = 0;
            depthDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            depthDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            VkSubpassDependency2 dependencies[] = { colorDependency, depthDependency };
            renderPassCreateInfo.dependencyCount = 2;
            renderPassCreateInfo.pDependencies = dependencies;

            VK_CHECK(vkCreateRenderPass2(device->device, &renderPassCreateInfo, nullptr, &renderPass))

            imageLayouts.resize(imageCount);
            imageViews.resize(imageCount);
            frameBuffers.resize(imageCount);
            for(size_t i = 0; i < images.size(); i++) {
                VkImageViewCreateInfo imageViewCreateInfo{};
                imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                imageViewCreateInfo.image = images[i];
                imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                imageViewCreateInfo.format = surfaceFormat.format;
                imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
                imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
                imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
                imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
                imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
                imageViewCreateInfo.subresourceRange.levelCount = 1;
                imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
                imageViewCreateInfo.subresourceRange.layerCount = 1;

                VK_CHECK(vkCreateImageView(device->device, &imageViewCreateInfo, nullptr, &imageViews[i]))

                // We can use a single depth texture for all frame buffers
                VkImageView attachments[] = { imageViews[i], depthImageView };
                VkFramebufferCreateInfo frameBufferInfo = {};
                frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                frameBufferInfo.renderPass = renderPass;
                frameBufferInfo.attachmentCount = 2;
                frameBufferInfo.pAttachments = attachments;
                frameBufferInfo.width = extent.width;
                frameBufferInfo.height = extent.height;
                frameBufferInfo.layers = 1;
                VK_CHECK(vkCreateFramebuffer(device->device, &frameBufferInfo, nullptr, &frameBuffers[i]));

                imageLayouts[i] = VK_IMAGE_LAYOUT_UNDEFINED;
            }

            depthImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            isComplete = true;

        }

        SwapChain::~SwapChain() {

            if (!isComplete) return;

            for (auto& frameBuffer : frameBuffers) {
                vkDestroyFramebuffer(device->device, frameBuffer, nullptr);
            }

            for (auto& imageView : imageViews) {
                vkDestroyImageView(device->device, imageView, nullptr);
            }

            vkDestroyImageView(device->device, depthImageView, nullptr);
            vmaDestroyImage(device->memoryManager->allocator,
                depthImageAllocation.image, depthImageAllocation.allocation);

            vkDestroyRenderPass(device->device, renderPass, nullptr);
            vkDestroySwapchainKHR(device->device, swapChain, nullptr);

        }

        bool SwapChain::AcquireImageIndex(VkSemaphore semaphore) {

            if (!isComplete) return false;

            auto result = vkAcquireNextImageKHR(device->device, swapChain, 1000000000,
                semaphore, nullptr, &aquiredImageIndex);

            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) return true;
            VK_CHECK(result);

            return false;

        }

        bool SwapChain::IsHDR() const {

            switch(surfaceFormat.colorSpace) {
                case VK_COLOR_SPACE_HDR10_HLG_EXT:
                case VK_COLOR_SPACE_DOLBYVISION_EXT:
                case VK_COLOR_SPACE_HDR10_ST2084_EXT:
                    return true;
                default:
                    return false;
            }

        }

        bool SwapChain::NeedsGammaCorrection() const {

            switch(surfaceFormat.colorSpace) {
                case VK_COLOR_SPACE_DCI_P3_LINEAR_EXT:
                case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:
                    return false;
                default:
                    return true;
            }

        }

        VkSurfaceFormatKHR SwapChain::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats,
            bool preferHDRSurface) {

            VkSurfaceFormatKHR selectedFormat = formats.front();

            bool isSelectedFormatHDR = false;

            for (const auto& availableFormat : formats) {

                if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
                    availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR &&
                    !isSelectedFormatHDR) {
                    selectedFormat = availableFormat;
                }

                if (preferHDRSurface) {
                    // Prefer HDR10 HLG
                    if (availableFormat.format == VK_FORMAT_R16G16B16A16_SFLOAT &&
                        availableFormat.colorSpace == VK_COLOR_SPACE_HDR10_HLG_EXT) {
                        selectedFormat = availableFormat;
                        isSelectedFormatHDR = true;
                    }

                    if (availableFormat.format == VK_FORMAT_R16G16B16A16_SFLOAT &&
                        availableFormat.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT &&
                        selectedFormat.colorSpace != VK_COLOR_SPACE_HDR10_HLG_EXT) {
                        selectedFormat = availableFormat;
                        isSelectedFormatHDR = true;
                    }

                    if (availableFormat.format == VK_FORMAT_R16G16B16A16_SFLOAT &&
                        availableFormat.colorSpace == VK_COLOR_SPACE_DOLBYVISION_EXT &&
                        selectedFormat.colorSpace != VK_COLOR_SPACE_HDR10_HLG_EXT) {
                        selectedFormat = availableFormat;
                        isSelectedFormatHDR = true;
                    }
                }
            }

            return selectedFormat;

        }

        VkPresentModeKHR SwapChain::ChoosePresentMode(const std::vector<VkPresentModeKHR> &presentModes,
                                                      VkPresentModeKHR preferenceMode) {

            for (const auto& availablePresentMode : presentModes) {
                if (availablePresentMode == preferenceMode) {
                    return availablePresentMode;
                }
            }

            return VK_PRESENT_MODE_FIFO_KHR;

        }

        VkExtent2D SwapChain::ChooseExtent(VkSurfaceCapabilitiesKHR capabilities,
            int32_t desiredWidth, int32_t desiredHeight) {

            if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
                return capabilities.currentExtent;
            }

            VkExtent2D actualExtent = {
                    static_cast<uint32_t>(desiredWidth),
                    static_cast<uint32_t>(desiredHeight)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;

        }

    }

}