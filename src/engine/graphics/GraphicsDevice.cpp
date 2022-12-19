#include "GraphicsDevice.h"
#include "Instance.h"
#include "../EngineInstance.h"

#include <vector>
#include <map>
#include <cassert>
#include <set>

namespace Atlas {

    namespace Graphics {

        GraphicsDevice::GraphicsDevice(Surface* surface, bool enableValidationLayers) : surface(surface) {

            auto instance = EngineInstance::GetGraphicsInstance();

            const std::vector<const char*> requiredExtensions = {
                    VK_KHR_SWAPCHAIN_EXTENSION_NAME
#ifdef AE_OS_MACOS
                    , "VK_KHR_portability_subset"
#endif
            };

            SelectPhysicalDevice(instance->instance,
                surface->GetNativeSurface(), requiredExtensions);

            vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

            float priority = 1.0f;
            auto queueCreateInfos = CreateQueueInfos(&priority);

            VkPhysicalDeviceFeatures deviceFeatures{};
            deviceFeatures.samplerAnisotropy = VK_TRUE;

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
            vkGetDeviceQueue(device, queueFamilyIndices.queueFamilies[QueueType::TransferQueue].value(), 0,
                &queueFamilyIndices.queues[QueueType::TransferQueue]);

            memoryManager = new MemoryManager(instance->instance, physicalDevice, device,
                queueFamilyIndices.queueFamilies[QueueType::TransferQueue].value(),
                queueFamilyIndices.queues[QueueType::TransferQueue]);

            CreateSwapChain();
            CreateFrameData();

            // Acquire first index since normally these are acquired at completion of frame
            swapChain->AcquireImageIndex();

            isComplete = true;

        }

        GraphicsDevice::~GraphicsDevice() {

            // Make sure that all commands were processed before
            // deleting resources
            WaitForIdle();

            delete swapChain;

            // The whole deletion thing should be reworked a bit
            for (auto shader : shaders) {
                delete shader;
            }

            for (auto pipeline : pipelines) {
                delete pipeline;
            }

            for (auto buffer : buffers) {
                delete buffer;
            }

            for (auto image : images) {
                delete image;
            }

            for (auto sampler : samplers) {
                delete sampler;
            }

            for (auto pool : descriptorPools) {
                delete pool;
            }

            DestroyFrameData();

            // Delete memory manager at last, since it needs to clean up
            // all remaining memory which was destroyed by buffers, images, etc.
            delete memoryManager;
            vkDestroyDevice(device, nullptr);

        }

        Shader* GraphicsDevice::CreateShader(ShaderDesc shaderDesc) {

            auto shader = new Shader(memoryManager, shaderDesc);

            shaders.push_back(shader);

            return shader;

        }

        Pipeline* GraphicsDevice::CreatePipeline(GraphicsPipelineDesc desc) {

            auto pipeline = new Pipeline(memoryManager, desc);

            pipelines.push_back(pipeline);

            return pipeline;

        }

        Pipeline* GraphicsDevice::CreatePipeline(ComputePipelineDesc desc) {

            auto pipeline = new Pipeline(memoryManager, desc);

            pipelines.push_back(pipeline);

            return pipeline;

        }

        Buffer* GraphicsDevice::CreateBuffer(BufferDesc bufferDesc) {

            auto buffer = new Buffer(memoryManager, bufferDesc);

            buffers.push_back(buffer);

            return buffer;

        }

        Image* GraphicsDevice::CreateImage(ImageDesc imageDesc) {

            auto image = new Image(this, imageDesc);

            images.push_back(image);

            return image;

        }

        Sampler* GraphicsDevice::CreateSampler(SamplerDesc samplerDesc) {

            auto sampler = new Sampler(this, samplerDesc);

            samplers.push_back(sampler);

            return sampler;

        }

        DescriptorPool* GraphicsDevice::CreateDescriptorPool() {

            auto pool = new DescriptorPool(this);

            descriptorPools.push_back(pool);

            return pool;

        }

        CommandList* GraphicsDevice::GetCommandList(QueueType queueType) {

            auto frameData = GetFrameData();

            // Lock to find a command list or create a new one
            std::lock_guard<std::mutex> lock(frameData->commandListsMutex);

            auto& commandLists = frameData->commandLists;
            auto it = std::find_if(commandLists.begin(), commandLists.end(),
                [&](CommandList* commandList) {
                    if (commandList->queueType == queueType) {
                        bool expected = false;
                        // Check if isLocked is set to false and if so, set it to true
                        // In that case we can use this command list, otherwise continue
                        // the search for another one
                        if (commandList->isLocked.compare_exchange_strong(expected, true)) {
                            return true;
                        }
                    }
                    return false;
                });

            if (it == commandLists.end()) {
                auto queueFamilyIndex = queueFamilyIndices.queueFamilies[queueType];
                CommandList* cmd = new CommandList(this, queueType, queueFamilyIndex.value());
                commandLists.push_back(cmd);
                return cmd;
            }

            return *it;

        }

        void GraphicsDevice::SubmitCommandList(CommandList *cmd, VkPipelineStageFlags waitStage, ExecutionOrder order) {

            // After the submission of a command list, we don't unlock it anymore
            // for further use in this frame. Instead, we will unlock it again
            // when we get back to this frames data and start a new frame with it.
            auto frameData = GetFrameData();

            std::vector<VkSemaphore> waitSemaphores = { swapChain->semaphore };
            std::vector<VkPipelineStageFlags> waitStages = { waitStage };
            if (frameData->submittedCommandLists.size() > 0) {
                waitSemaphores[0] = frameData->submittedCommandLists.back()->semaphore;
            }

            VkSubmitInfo submit = {};
            submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit.pNext = nullptr;
            submit.pWaitDstStageMask = waitStages.data();
            submit.waitSemaphoreCount = uint32_t(waitSemaphores.size());
            submit.pWaitSemaphores = waitSemaphores.data();
            submit.signalSemaphoreCount = 1;
            submit.pSignalSemaphores = &cmd->semaphore;
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &cmd->commandBuffer;

            auto queue = queueFamilyIndices.queues[cmd->queueType];
            VK_CHECK(vkQueueSubmit(queue, 1, &submit, cmd->fence))

            frameData->submittedCommandLists.push_back(cmd);

        }

        void GraphicsDevice::CompleteFrame() {

            bool recreateSwapChain = false;

            auto frameData = GetFrameData();
            std::vector<VkSemaphore> semaphores;
            // For now, we will only use sequential execution of queue submits,
            // which means only the latest submit can signal its semaphore here
            //for (auto cmd : frameData->submittedCommandLists)
            //    semaphores.push_back(cmd->semaphore);
            semaphores.push_back(frameData->submittedCommandLists.back()->semaphore);

            VkPresentInfoKHR presentInfo = {};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.pNext = nullptr;
            presentInfo.pSwapchains = &swapChain->swapChain;
            presentInfo.swapchainCount = 1;
            presentInfo.pWaitSemaphores = semaphores.data();
            presentInfo.waitSemaphoreCount = uint32_t(semaphores.size());
            presentInfo.pImageIndices = &swapChain->aquiredImageIndex;

            VkQueue& presenterQueue = queueFamilyIndices.queues[QueueType::PresentationQueue];
            auto result = vkQueuePresentKHR(presenterQueue, &presentInfo);

            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
                recreateSwapChain = true;
            }
            else {
                VK_CHECK(result)
            }

            // Delete data that is marked for deletion for this frame
            memoryManager->DeleteData();
            frameIndex++;
            memoryManager->UpdateFrameIndex(frameIndex);

            auto nextFrameData = GetFrameData();
            nextFrameData->WaitAndReset(device);

            if (swapChain->AcquireImageIndex()) {
                recreateSwapChain = true;
            }

            if (recreateSwapChain || CheckForWindowResize()) {
                vkDeviceWaitIdle(device);
                CreateSwapChain();
                // Acquire a new image index for new swap chain
                swapChain->AcquireImageIndex();
            }

        }

        bool GraphicsDevice::CheckFormatSupport(VkFormat format, VkFormatFeatureFlags featureFlags) {

            VkFormatProperties properties;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
            return (properties.optimalTilingFeatures & featureFlags) == featureFlags;

        }

        VkQueue GraphicsDevice::GetQueue(Atlas::Graphics::QueueType queueType) const {

            return queueFamilyIndices.queues[queueType];

        }

        void GraphicsDevice::WaitForIdle() const {

            vkDeviceWaitIdle(device);

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

                if (!deviceFeatures.samplerAnisotropy) {
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
                bool isGraphicsFamilyValid = true;
                bool isTransferFamilyValid = true;

                if (!(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
                    isGraphicsFamilyValid = false;

                if (!(queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT))
                    isGraphicsFamilyValid = false;

                if (!(queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT))
                    isTransferFamilyValid = false;

                if (isGraphicsFamilyValid) {
                    queueFamilyIndices.queueFamilies[QueueType::GraphicsQueue] = counter;
                }

                if (isTransferFamilyValid) {
                    queueFamilyIndices.queueFamilies[QueueType::TransferQueue] = counter;
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

        void GraphicsDevice::CreateSwapChain() {

            auto nativeSurface = surface->GetNativeSurface();
            auto nativeWindow = surface->GetNativeWindow();

            auto supportDetails = SwapChainSupportDetails(physicalDevice, nativeSurface);

            int32_t width, height;
            SDL_GL_GetDrawableSize(nativeWindow, &width, &height);

            windowWidth = width;
            windowHeight = height;

            auto oldSwapChain = swapChain;

            swapChain = new SwapChain(supportDetails, nativeSurface, memoryManager,
                width, height, VK_PRESENT_MODE_FIFO_KHR, oldSwapChain);

            // Clean up old swap chain
            delete oldSwapChain;

        }

        bool GraphicsDevice::CheckForWindowResize() {

            auto nativeWindow = surface->GetNativeWindow();

            int32_t width, height;
            SDL_GL_GetDrawableSize(nativeWindow, &width, &height);

            if (width != windowWidth || height != windowHeight) {
                windowWidth = width;
                windowHeight = height;
                return true;
            }

            return false;

        }

        void GraphicsDevice::CreateFrameData() {

            VkFenceCreateInfo fenceInfo = Initializers::InitFenceCreateInfo();
            for (int32_t i = 0; i < FRAME_DATA_COUNT; i++) {
                // Create secondary fences in a signaled state
                if (i > 0) fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
                VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &frameData[i].fence))
            }

        }

        void GraphicsDevice::DestroyFrameData() {

            for (int32_t i = 0; i < FRAME_DATA_COUNT; i++) {
                auto& commandLists = frameData[i].commandLists;
                for (auto commandList : commandLists) {
                    delete commandList;
                }

                vkDestroyFence(device, frameData[i].fence, nullptr);
            }

        }

        FrameData *GraphicsDevice::GetFrameData() {

            return &frameData[frameIndex % FRAME_DATA_COUNT];

        }

    }

}