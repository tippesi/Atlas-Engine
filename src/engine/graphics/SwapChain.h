#ifndef AE_GRAPHICSSWAPCHAIN_H
#define AE_GRAPHICSSWAPCHAIN_H

#include "Common.h"
#include "Surface.h"

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
                      VkDevice device, int desiredWidth, int32_t desiredHeight, bool& success,
                      VkPresentModeKHR desiredMode = VK_PRESENT_MODE_FIFO_KHR,
                      SwapChain* oldSwapchain = nullptr);

            ~SwapChain();

            VkSwapchainKHR swapChain;
            VkSurfaceFormatKHR surfaceFormat;
            VkPresentModeKHR presentMode;
            VkExtent2D extent;


            std::vector<VkImage> images;
            std::vector<VkImageView> imageViews;
            std::vector<VkFramebuffer> frameBuffers;
            VkRenderPass defaultRenderPass;

        private:
            VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
            VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& presentModes,
                                               VkPresentModeKHR desiredMode);
            VkExtent2D ChooseExtent(VkSurfaceCapabilitiesKHR capabilities,
                                    int32_t desiredWidth, int32_t desiredHeight);

            VkDevice device;

        };

    }

}


#endif
