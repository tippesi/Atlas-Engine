#include "GraphicsDevice.h"
#include "Instance.h"
#include "../EngineInstance.h"

#include <vector>
#include <map>
#include <cassert>
#include <set>

namespace Atlas {

    namespace Graphics {

        GraphicsDevice::GraphicsDevice(Surface* surface, bool &success, bool enableValidationLayers) {

            auto instance = EngineInstance::GetGraphicsInstance();

            success = SelectPhysicalDevice(surface, instance->instance);
            assert(success && "Error selecting physical device");

            auto queueCreateInfos = CreateQueueInfos();

            VkPhysicalDeviceFeatures deviceFeatures{};

            VkDeviceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            createInfo.pQueueCreateInfos = queueCreateInfos.data();
            createInfo.queueCreateInfoCount = uint32_t(queueCreateInfos.size());

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

            vkGetDeviceQueue(device, queueFamilyIndices.graphicsFamily.value(), 0,
                             &queueFamilyIndices.graphicsQueue);
            vkGetDeviceQueue(device, queueFamilyIndices.presentationFamily.value(), 0,
                             &queueFamilyIndices.presentationQueue);

        }

        GraphicsDevice::~GraphicsDevice() {

            vkDestroyDevice(device, nullptr);

        }

        bool GraphicsDevice::SelectPhysicalDevice(Surface* surface, VkInstance instance) {

            uint32_t deviceCount = 0;
            bool success = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr) == VK_SUCCESS;
            assert(success && deviceCount && "Couldn't find any physical devices");
            if (!success || !deviceCount) return false;

            std::vector<VkPhysicalDevice> devices(deviceCount);
            success &= vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()) == VK_SUCCESS;
            assert(success && "Error retrieving physical devices");

            std::multimap<int32_t, VkPhysicalDevice> candidates;
            for (const auto& device : devices) {
                int32_t score = RateDeviceSuitability(surface, device);
                candidates.insert(std::make_pair(score, device));
            }

            auto foundSuitableDevice = candidates.rbegin()->first > 0;
            assert(foundSuitableDevice && "No suitable device found");
            // Check if the best candidate is suitable at all
            if (foundSuitableDevice) {
                physicalDevice = candidates.rbegin()->second;
            } else {
                return false;
            }

            FindQueueFamilies(surface);
            auto completeIndices = queueFamilyIndices.IsComplete();
            assert(completeIndices && "No valid queue family found");
            if (!completeIndices) {
                return false;
            }

            return true;

        }

        int32_t GraphicsDevice::RateDeviceSuitability(Surface* surface, VkPhysicalDevice device) {

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

        bool GraphicsDevice::FindQueueFamilies(Surface* surface) {

            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

            auto vkSurface = surface->GetNativeSurface();

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
                    queueFamilyIndices.graphicsFamily = counter;
                }

                VkBool32 presentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, counter, vkSurface, &presentSupport);
                if (presentSupport) {
                    queueFamilyIndices.presentationFamily = counter;
                }

                if (queueFamilyIndices.IsComplete()) return true;

                counter++;
            }

            return false;

        }

        std::vector<VkDeviceQueueCreateInfo> GraphicsDevice::CreateQueueInfos() {

            std::vector<VkDeviceQueueCreateInfo> queueInfos;
            std::set<uint32_t> queueFamilies = {
                    queueFamilyIndices.graphicsFamily.value(),
                    queueFamilyIndices.presentationFamily.value()
            };

            float priority = 1.0f;
            for (auto queueFamily : queueFamilies) {
                VkDeviceQueueCreateInfo queueCreateInfo{};
                queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueCreateInfo.queueFamilyIndex = queueFamily;
                queueCreateInfo.queueCount = 1;
                queueCreateInfo.pQueuePriorities = &priority;
                queueInfos.push_back(queueCreateInfo);
            }

            return queueInfos;

        }

    }

}