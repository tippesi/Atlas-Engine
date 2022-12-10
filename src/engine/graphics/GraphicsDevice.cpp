#include "GraphicsDevice.h"
#include "Instance.h"
#include "../EngineInstance.h"

#include <vector>
#include <map>
#include <cassert>
#include <set>

namespace Atlas {

    namespace Graphics {

        GraphicsDevice::GraphicsDevice(Surface* surface, bool enableValidationLayers) {

            auto instance = EngineInstance::GetGraphicsInstance();

            const std::vector<const char*> requiredExtensions = {
                    VK_KHR_SWAPCHAIN_EXTENSION_NAME
#ifdef AE_OS_MACOS
                    , "VK_KHR_portability_subset"
#endif
            };

            SelectPhysicalDevice(instance->instance,
                surface->GetNativeSurface(), requiredExtensions);

            float priority = 1.0f;
            auto queueCreateInfos = CreateQueueInfos(&priority);

            VkPhysicalDeviceFeatures deviceFeatures{};

            VkDeviceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            createInfo.pQueueCreateInfos = queueCreateInfos.data();
            createInfo.queueCreateInfoCount = uint32_t(queueCreateInfos.size());

            createInfo.pEnabledFeatures = &deviceFeatures;
            createInfo.enabledExtensionCount = uint32_t(requiredExtensions.size());
            createInfo.ppEnabledExtensionNames = requiredExtensions.data();

            if (enableValidationLayers) {
                createInfo.enabledLayerCount = uint32_t(instance->layerNames.size());
                createInfo.ppEnabledLayerNames = instance->layerNames.data();
            } else {
                createInfo.enabledLayerCount = 0;
            }

            VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device))

            vkGetDeviceQueue(device, queueFamilyIndices.queueFamilies[QueueType::GraphicsQueue].value(), 0,
                             &queueFamilyIndices.queues[QueueType::GraphicsQueue]);
            vkGetDeviceQueue(device, queueFamilyIndices.queueFamilies[QueueType::PresentationQueue].value(), 0,
                             &queueFamilyIndices.queues[QueueType::PresentationQueue]);

            CreateSwapChain(surface);
            CreateFrameData();

            isComplete = true;

        }

        GraphicsDevice::~GraphicsDevice() {

            // Make sure that all commands were processed before
            // deleting resources
            vkDeviceWaitIdle(device);

            delete swapChain;

            for (auto commandList : commandLists) {
                delete commandList;
            }

            DestroyFrameData();

            vkDestroyDevice(device, nullptr);

        }

        Shader* GraphicsDevice::CreateShader(ShaderDesc shaderDesc) {



        }

        CommandList* GraphicsDevice::GetCommandList(QueueType queueType) {

            auto it = std::find_if(commandLists.begin(), commandLists.end(),
                [&](CommandList* commandList) {
                    return commandList->queueType == queueType;
                });

            if (it == commandLists.end()) {
                auto queueFamilyIndex = queueFamilyIndices.queueFamilies[queueType];
                CommandList* cmd = new CommandList(device, queueType, queueFamilyIndex.value());
                commandLists.push_back(cmd);
                return cmd;
            }

            return *it;

        }

        void GraphicsDevice::SubmitCommandList(CommandList *cmd) {

            VkSubmitInfo submit = {};
            submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit.pNext = nullptr;

            VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

            submit.pWaitDstStageMask = &waitStage;
            submit.waitSemaphoreCount = 1;
            submit.pWaitSemaphores = &swapChain->semaphore;
            submit.signalSemaphoreCount = 1;
            submit.pSignalSemaphores = &cmd->semaphore;
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &cmd->commandBuffer;

            auto queue = queueFamilyIndices.queues[cmd->queueType];
            auto frameData = GetFrameData();
            VK_CHECK(vkQueueSubmit(queue, 1, &submit, frameData->fence));

        }

        void GraphicsDevice::CompleteFrame() {

            std::vector<VkSemaphore> semaphores;
            for (auto cmd : commandLists) semaphores.push_back(cmd->semaphore);

            VkPresentInfoKHR presentInfo = {};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.pNext = nullptr;

            presentInfo.pSwapchains = &swapChain->swapChain;
            presentInfo.swapchainCount = 1;

            presentInfo.pWaitSemaphores = semaphores.data();
            presentInfo.waitSemaphoreCount = uint32_t(semaphores.size());

            presentInfo.pImageIndices = &swapChain->aquiredImageIndex;

            VkQueue& presenterQueue = queueFamilyIndices.queues[QueueType::PresentationQueue];
            VK_CHECK(vkQueuePresentKHR(presenterQueue, &presentInfo));

            frameIndex++;

            auto frameData = GetFrameData();
            // Need to check if we can use the next frame data
            VK_CHECK(vkWaitForFences(device, 1, &frameData->fence, true, 1000000000))
            VK_CHECK(vkResetFences(device, 1, &frameData->fence))

        }

        bool GraphicsDevice::SelectPhysicalDevice(VkInstance instance, VkSurfaceKHR surface,
            const std::vector<const char*>& requiredExtensions) {

            uint32_t deviceCount = 0;
            VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr));
            if (!deviceCount) return false;

            std::vector<VkPhysicalDevice> devices(deviceCount);
            VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()))

            std::multimap<int32_t, VkPhysicalDevice> candidates;
            for (const auto& device : devices) {
                int32_t score = RateDeviceSuitability(device, surface, requiredExtensions);
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

            // The following is done to see the reason in terms of a
            {
                FindQueueFamilies(physicalDevice, surface);
                auto completeIndices = queueFamilyIndices.IsComplete();
                assert(completeIndices && "No valid queue family found");
                if (!completeIndices) {
                    return false;
                }

                auto extensionsSupported = CheckDeviceExtensionSupport(physicalDevice, requiredExtensions);
                assert(extensionsSupported && "Some required extensions are not supported");
                if (!extensionsSupported) {
                    return false;
                }

            }

            return true;

        }

        int32_t GraphicsDevice::RateDeviceSuitability(VkPhysicalDevice device, VkSurfaceKHR surface,
            const std::vector<const char*>& requiredExtensions) {

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

            // The following are the requirements we MUST meet
            {
                FindQueueFamilies(device, surface);
                if (!queueFamilyIndices.IsComplete()) {
                    return 0;
                }

                // Need to check extensions before checking the swap chain details
                auto extensionsSupported = CheckDeviceExtensionSupport(device, requiredExtensions);
                if (!extensionsSupported) {
                    return 0;
                }

                auto swapchainSupportDetails = SwapChainSupportDetails(device, surface);
                if (!swapchainSupportDetails.IsAdequate()) {
                    return 0;
                }
            }

            return score;

        }

        bool GraphicsDevice::FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {

            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

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
                    queueFamilyIndices.queueFamilies[QueueType::GraphicsQueue] = counter;
                }

                VkBool32 presentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, counter, surface, &presentSupport);
                if (presentSupport) {
                    queueFamilyIndices.queueFamilies[QueueType::PresentationQueue] = counter;
                }

                if (queueFamilyIndices.IsComplete()) return true;

                counter++;
            }



            return false;

        }

        std::vector<VkDeviceQueueCreateInfo> GraphicsDevice::CreateQueueInfos(float* priority) {

            std::vector<VkDeviceQueueCreateInfo> queueInfos;
            std::set<uint32_t> queueFamilies = {
                    queueFamilyIndices.queueFamilies[QueueType::GraphicsQueue].value(),
                    queueFamilyIndices.queueFamilies[QueueType::PresentationQueue].value()
            };

            for (auto queueFamily : queueFamilies) {
                VkDeviceQueueCreateInfo queueCreateInfo{};
                queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueCreateInfo.queueFamilyIndex = queueFamily;
                queueCreateInfo.queueCount = 1;
                queueCreateInfo.pQueuePriorities = priority;
                queueInfos.push_back(queueCreateInfo);
            }

            return queueInfos;

        }

        bool GraphicsDevice::CheckDeviceExtensionSupport(VkPhysicalDevice physicalDevice,
            const std::vector<const char *>& extensionNames) {

            uint32_t extensionCount;
            vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

            std::vector<VkExtensionProperties> availableExtensions(extensionCount);
            vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

            std::set<std::string> requiredExtensions(extensionNames.begin(), extensionNames.end());

            for (const auto& extension : availableExtensions) {
                requiredExtensions.erase(extension.extensionName);
            }
            assert(requiredExtensions.empty() && "Not all required extensions were found");

            return requiredExtensions.empty();

        }

        void GraphicsDevice::CreateSwapChain(Surface *surface) {

            auto nativeSurface = surface->GetNativeSurface();
            auto nativeWindow = surface->GetNativeWindow();

            auto supportDetails = SwapChainSupportDetails(physicalDevice, nativeSurface);

            int32_t width, height;
            SDL_GL_GetDrawableSize(nativeWindow, &width, &height);

            swapChain = new SwapChain(supportDetails, nativeSurface, device,
                width, height,
                VK_PRESENT_MODE_FIFO_KHR, nullptr);

        }

        void GraphicsDevice::CreateFrameData() {

            VkFenceCreateInfo fenceInfo = Initializers::InitFenceCreateInfo();
            for (int32_t i = 0; i < FRAME_DATA_COUNT; i++) {
                VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &frameData[i].fence))
            }

        }

        void GraphicsDevice::DestroyFrameData() {

            for (int32_t i = 0; i < FRAME_DATA_COUNT; i++) {
                vkDestroyFence(device, frameData[i].fence, nullptr);
            }

        }

        const FrameData *GraphicsDevice::GetFrameData() const {

            return &frameData[frameIndex % FRAME_DATA_COUNT];

        }

    }

}