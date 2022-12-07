#ifndef AE_GRAPHICSDEVICE_H
#define AE_GRAPHICSDEVICE_H

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <optional>

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

            GraphicsDevice(Instance* instance, bool& success, bool enableValidationLayers = false);

            ~GraphicsDevice();

            

        private:
            struct QueueFamilyIndices {
                std::optional<uint32_t> indices;

                bool IsComplete() {
                    return indices.has_value();
                }
            };
            bool SelectPhysicalDevice(VkInstance instance);

            int32_t RateDeviceSuitability(VkPhysicalDevice device);

            bool FindQueueFamilies();

            VkPhysicalDevice physicalDevice;
            VkDevice device;

            QueueFamilyIndices queueFamilyIndices;

        };

    }

}

#endif
