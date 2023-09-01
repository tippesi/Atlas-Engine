#pragma once

#include "Common.h"
#include "Surface.h"
#include "MemoryManager.h"

#include <vector>
#include <cstdint>

namespace Atlas {

    namespace Graphics {

        enum ColorSpace {
            SRGB_NONLINEAR = 0,
            HDR10_HLG,
            HDR10_ST2084,
            DOLBY_VISION,
            OTHER
        };

        class SwapChainSupportDetails {

        public:
            SwapChainSupportDetails(VkPhysicalDevice device, VkSurfaceKHR surface);

            bool IsAdequate() const;

            VkSurfaceCapabilitiesKHR capabilities;
            std::vector<VkSurfaceFormatKHR> formats;
            std::vector<VkPresentModeKHR> presentModes;

        };

        struct SwapChainDesc {

        };

        class GraphicsDevice;

        class SwapChain {

        public:
            SwapChain(const SwapChainSupportDetails& supportDetails, VkSurfaceKHR surface,
                GraphicsDevice* Device, int32_t desiredWidth, int32_t desiredHeight,
                ColorSpace preferredColorSpace = SRGB_NONLINEAR,
                VkPresentModeKHR desiredMode = VK_PRESENT_MODE_FIFO_KHR,
                SwapChain* oldSwapchain = nullptr);

            ~SwapChain();

            bool AcquireImageIndex(VkSemaphore semaphore);

            bool IsHDR() const;

            bool NeedsGammaCorrection() const;

            VkSwapchainKHR swapChain;
            VkSurfaceFormatKHR surfaceFormat;
            VkPresentModeKHR presentMode;
            VkExtent2D extent;

            ColorSpace colorSpace;

            uint32_t aquiredImageIndex = 0;
            std::vector<VkImage> images;
            std::vector<VkImageLayout> imageLayouts;
            std::vector<VkImageView> imageViews;
            std::vector<VkFramebuffer> frameBuffers;

            ImageAllocation depthImageAllocation;
            VkImageLayout depthImageLayout;
            VkImageView depthImageView;

            VkRenderPass renderPass;

            VkClearValue colorClearValue = { .color = { { 1.0f, 1.0f, 1.0f, 1.0f } } };
            VkClearValue depthClearValue = { .depthStencil = { .depth = 1.0f } };

            bool isComplete = false;

        private:
            VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats,
                ColorSpace preferredColorSpace);
            VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& presentModes,
                                               VkPresentModeKHR desiredMode);
            VkExtent2D ChooseExtent(VkSurfaceCapabilitiesKHR capabilities,
                                    int32_t desiredWidth, int32_t desiredHeight);

            GraphicsDevice* device;

        };

    }

}