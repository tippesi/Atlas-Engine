#ifndef AE_GRAPHICSDEVICE_H
#define AE_GRAPHICSDEVICE_H

#include "Surface.h"

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

            bool SelectPhysicalDevice(Surface* surface, VkInstance instance);

            int32_t RateDeviceSuitability(Surface* surface, VkPhysicalDevice device);

            bool FindQueueFamilies(Surface* surface);

            std::vector<VkDeviceQueueCreateInfo> CreateQueueInfos();

            VkPhysicalDevice physicalDevice;
            VkDevice device;

            QueueFamilyIndices queueFamilyIndices;

        };

    }

}

#endif
