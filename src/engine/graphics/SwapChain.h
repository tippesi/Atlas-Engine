#ifndef AE_GRAPHICSSWAPCHAIN_H
#define AE_GRAPHICSSWAPCHAIN_H

#include "Common.h"
#include "Surface.h"
#include "MemoryManager.h"

#include <vector>
#include <cstdint>

namespace Atlas {

    namespace Graphics {

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

        class SwapChain {

        public:
            SwapChain(const SwapChainSupportDetails& supportDetails, VkSurfaceKHR surface,
                      MemoryManager* memManager, int desiredWidth, int32_t desiredHeight,
                      VkPresentModeKHR desiredMode = VK_PRESENT_MODE_FIFO_KHR,
                      SwapChain* oldSwapchain = nullptr);

            ~SwapChain();

            bool AcquireImageIndex();

            VkSwapchainKHR swapChain;
            VkSurfaceFormatKHR surfaceFormat;
            VkPresentModeKHR presentMode;
            VkExtent2D extent;

            uint32_t aquiredImageIndex = 0;
            std::vector<VkImage> images;
            std::vector<VkImageLayout> imageLayouts;
            std::vector<VkImageView> imageViews;
            std::vector<VkFramebuffer> frameBuffers;

            ImageAllocation depthImageAllocation;
            VkImageLayout depthImageLayout;
            VkImageView depthImageView;

            VkRenderPass defaultRenderPass;
            VkSemaphore semaphore;

            VkClearValue colorClearValue = { .color = { { 1.0f, 1.0f, 1.0f, 1.0f } } };
            VkClearValue depthClearValue = { .depthStencil = { .depth = 1.0f } };

        private:
            VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
            VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& presentModes,
                                               VkPresentModeKHR desiredMode);
            VkExtent2D ChooseExtent(VkSurfaceCapabilitiesKHR capabilities,
                                    int32_t desiredWidth, int32_t desiredHeight);

            MemoryManager* memoryManager;
            VkDevice device;

        };

    }

}


#endif
