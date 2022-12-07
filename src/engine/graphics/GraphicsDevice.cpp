#include "GraphicsDevice.h"
#include "Instance.h"

#include <vector>
#include <map>

#include <volk.h>

namespace Atlas {

    namespace Graphics {

        GraphicsDevice::GraphicsDevice(Instance* instance, bool &success, bool enableValidationLayers) {

            success = SelectPhysicalDevice(instance->instance);
            success &= FindQueueFamilies();

            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamilyIndices.indices.value();
            queueCreateInfo.queueCount = 1;

            float queuePriority = 1.0f;
            queueCreateInfo.pQueuePriorities = &queuePriority;

            VkPhysicalDeviceFeatures deviceFeatures{};

            VkDeviceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            createInfo.pQueueCreateInfos = &queueCreateInfo;
            createInfo.queueCreateInfoCount = 1;

            const std::vector<const char*> requiredExtensions = {
                    "VK_KHR_portability_subset"
            };
            createInfo.pEnabledFeatures = &deviceFeatures;
            createInfo.enabledExtensionCount = uint32_t(requiredExtensions.size());
            createInfo.ppEnabledExtensionNames = requiredExtensions.data();

            if (enableValidationLayers) {
                createInfo.enabledLayerCount = uint32_t(instance->layerNames.size());
                createInfo.ppEnabledLayerNames = instance->layerNames.data();
            } else {
                createInfo.enabledLayerCount = 0;
            }

            success &= vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) == VK_SUCCESS;
            assert(success && "Unable to create virtual device");

        }

        GraphicsDevice::~GraphicsDevice() {

            vkDestroyDevice(device, nullptr);

        }

        bool GraphicsDevice::SelectPhysicalDevice(VkInstance instance) {

            uint32_t deviceCount = 0;
            bool success = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr) == VK_SUCCESS;
            assert(success && deviceCount && "Couldn't find any physical devices");
            if (!success || !deviceCount) return false;

            std::vector<VkPhysicalDevice> devices(deviceCount);
            success &= vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()) == VK_SUCCESS;
            assert(success && "Error retrieving physical devices");

            std::multimap<int32_t, VkPhysicalDevice> candidates;
            for (const auto& device : devices) {
                int32_t score = RateDeviceSuitability(device);
                candidates.insert(std::make_pair(score, device));
            }

            // Check if the best candidate is suitable at all
            if (candidates.rbegin()->first > 0) {
                physicalDevice = candidates.rbegin()->second;
            } else {
                assert("No suitable device found");
                return false;
            }

            FindQueueFamilies();

            if (!queueFamilyIndices.IsComplete()) {
                assert("No valid queue family found");
                return false;
            }

            return true;

        }

        int32_t GraphicsDevice::RateDeviceSuitability(VkPhysicalDevice device) {

            int32_t score = 0;

            VkPhysicalDeviceProperties deviceProperties;
            VkPhysicalDeviceFeatures deviceFeatures;
            vkGetPhysicalDeviceProperties(device, &deviceProperties);
            vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

            // This property has to outweigh any other
            if (deviceProperties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                score += 10000;
            }

            // Maximum possible size of textures affects graphics quality
            score += deviceProperties.limits.maxImageDimension2D;

            return score;

        }

        bool GraphicsDevice::FindQueueFamilies() {

            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

            int32_t counter = 0;
            for (auto& queueFamily : queueFamilies) {
                bool isFamilyValid = true;

                if (!(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
                    isFamilyValid = false;

                if (!(queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT))
                    isFamilyValid = false;

                if (!(queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT))
                    isFamilyValid = false;

                if (isFamilyValid) {
                    queueFamilyIndices.indices = counter;
                    return true;
                }

                counter++;
            }

            return false;

        }

    }

}