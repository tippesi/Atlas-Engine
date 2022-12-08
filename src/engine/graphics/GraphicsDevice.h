#ifndef AE_GRAPHICSDEVICE_H
#define AE_GRAPHICSDEVICE_H

#include "Surface.h"
#include "SwapChain.h"

#include <volk.h>
#include <optional>
#include <vector>

namespace Atlas {

    namespace Graphics {

        class Instance;

        class GraphicsDevice {

        public:
            struct Queue {
                friend GraphicsDevice;
            private:
                VkQueue queue;
            };

            GraphicsDevice(Surface* surface, bool& success, bool enableValidationLayers = false);

            ~GraphicsDevice();

            SwapChain* swapChain = nullptr;

        private:
            struct QueueFamilyIndices {
                std::optional<uint32_t> graphicsFamily;
                std::optional<uint32_t> presentationFamily;

                VkQueue graphicsQueue;
                VkQueue presentationQueue;

                bool IsComplete() {
                    return graphicsFamily.has_value() && presentationFamily.has_value();
                }
            };

            bool SelectPhysicalDevice(VkInstance instance, VkSurfaceKHR surface,
                const std::vector<const char*>& requiredExtensions);

            int32_t RateDeviceSuitability(VkPhysicalDevice device, VkSurfaceKHR surface,
                const std::vector<const char*>& requiredExtensions);

            bool FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

            std::vector<VkDeviceQueueCreateInfo> CreateQueueInfos(float* priority);

            bool CheckDeviceExtensionSupport(VkPhysicalDevice physicalDevice,
                const std::vector<const char*>& extensionNames);

            bool CreateSwapChain(Surface* surface);

            VkPhysicalDevice physicalDevice;
            VkDevice device;

            QueueFamilyIndices queueFamilyIndices;

        };

    }

}

#endif
