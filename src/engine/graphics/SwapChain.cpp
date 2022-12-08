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
                             VkDevice device, int desiredWidth, int32_t desiredHeight, bool& success,
                             VkPresentModeKHR desiredMode, SwapChain* oldSwapchain) : device(device) {

            surfaceFormat = ChooseSurfaceFormat(supportDetails.formats);
            presentMode = ChoosePresentMode(supportDetails.presentModes, desiredMode);
            extent = ChooseExtent(supportDetails.capabilities, desiredWidth, desiredHeight);

            uint32_t imageCount = supportDetails.capabilities.minImageCount + 1;
            if (supportDetails.capabilities.maxImageCount > 0 &&
                supportDetails.capabilities.maxImageCount < imageCount) {
                imageCount = supportDetails.capabilities.maxImageCount;
            }

            VkSwapchainCreateInfoKHR createInfo{};
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

            success = vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) == VK_SUCCESS;
            assert(success && "Error creating swap chain");

            success &= vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr) == VK_SUCCESS;
            images.resize(imageCount);
            success &= vkGetSwapchainImagesKHR(device, swapChain, &imageCount, images.data()) == VK_SUCCESS;
            assert(success && "Error retrieving swap chain images");

            imageViews.resize(imageCount);

            for(int32_t i = 0; i < int32_t(images.size()); i++) {
                VkImageViewCreateInfo createInfo{};
                createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                createInfo.image = images[i];
                createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                createInfo.format = surfaceFormat.format;
                createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
                createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
                createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
                createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
                createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                createInfo.subresourceRange.baseMipLevel = 0;
                createInfo.subresourceRange.levelCount = 1;
                createInfo.subresourceRange.baseArrayLayer = 0;
                createInfo.subresourceRange.layerCount = 1;

                success &= vkCreateImageView(device, &createInfo, nullptr, &imageViews[i]) == VK_SUCCESS;
            }

        }

        SwapChain::~SwapChain() {

            for (auto& imageView : imageViews) {
                vkDestroyImageView(device, imageView, nullptr);
            }

            vkDestroySwapchainKHR(device, swapChain, nullptr);

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