#include "SwapChain.h"

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
                             VkDevice device, int desiredWidth, int32_t desiredHeight,
                             VkPresentModeKHR desiredMode, SwapChain* oldSwapchain) : device(device) {

            surfaceFormat = ChooseSurfaceFormat(supportDetails.formats);
            presentMode = ChoosePresentMode(supportDetails.presentModes, desiredMode);
            extent = ChooseExtent(supportDetails.capabilities, desiredWidth, desiredHeight);

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

            VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain))

            VK_CHECK(vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr))
            images.resize(imageCount);
            VK_CHECK(vkGetSwapchainImagesKHR(device, swapChain, &imageCount, images.data()))

            VkAttachmentDescription2 attachmentDescription = Initializers::InitAttachmentDescription(surfaceFormat.format,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
            VkAttachmentReference2 attachmentReference = Initializers::InitAttachmentReference(0,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            VkSubpassDescription2 subPassDescription = {};
            subPassDescription.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
            subPassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subPassDescription.colorAttachmentCount = 1;
            subPassDescription.pColorAttachments = &attachmentReference;

            VkRenderPassCreateInfo2 renderPassCreateInfo = Initializers::InitRenderPassCreateInfo(1,
                &attachmentDescription,1, &subPassDescription);
            VK_CHECK(vkCreateRenderPass2(device, &renderPassCreateInfo, nullptr, &defaultRenderPass))


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

                VK_CHECK(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &imageViews[i]))

                VkFramebufferCreateInfo frameBufferInfo = {};
                frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                frameBufferInfo.renderPass = defaultRenderPass;
                frameBufferInfo.attachmentCount = 1;
                frameBufferInfo.pAttachments = &imageViews[i];
                frameBufferInfo.width = extent.width;
                frameBufferInfo.height = extent.height;
                frameBufferInfo.layers = 1;
                VK_CHECK(vkCreateFramebuffer(device, &frameBufferInfo, nullptr, &frameBuffers[i]));

            }

            VkSemaphoreCreateInfo semaphoreInfo = Initializers::InitSemaphoreCreateInfo();
            VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore))

        }

        SwapChain::~SwapChain() {

            for (auto& frameBuffer : frameBuffers) {
                vkDestroyFramebuffer(device, frameBuffer, nullptr);
            }

            for (auto& imageView : imageViews) {
                vkDestroyImageView(device, imageView, nullptr);
            }

            vkDestroySemaphore(device, semaphore, nullptr);
            vkDestroyRenderPass(device, defaultRenderPass, nullptr);
            vkDestroySwapchainKHR(device, swapChain, nullptr);

        }

        void SwapChain::AquireImageIndex() {

            VK_CHECK(vkAcquireNextImageKHR(device, swapChain, 1000000000, semaphore, nullptr, &aquiredImageIndex));

        }

        VkSurfaceFormatKHR SwapChain::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats) {

            for (const auto& availableFormat : formats) {
                if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
                    availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                    return availableFormat;
                }
            }

            return formats.front();

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