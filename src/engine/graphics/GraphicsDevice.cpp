#include "GraphicsDevice.h"
#include "Instance.h"
#include "../EngineInstance.h"

#include <vector>
#include <map>
#include <cassert>
#include <set>

#include <vulkan/vulkan_beta.h>

namespace Atlas {

    namespace Graphics {

        GraphicsDevice* GraphicsDevice::DefaultDevice = nullptr;

        GraphicsDevice::GraphicsDevice(Surface* surface, bool enableValidationLayers) : surface(surface) {

            instance = Instance::DefaultInstance;

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

            BuildPhysicalDeviceFeatures(physicalDevice);

            VkDeviceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            createInfo.pQueueCreateInfos = queueCreateInfos.data();
            createInfo.queueCreateInfoCount = uint32_t(queueCreateInfos.size());

            createInfo.pEnabledFeatures = nullptr;
            createInfo.enabledExtensionCount = uint32_t(requiredExtensions.size());
            createInfo.ppEnabledExtensionNames = requiredExtensions.data();

            if (enableValidationLayers) {
                createInfo.enabledLayerCount = uint32_t(instance->layerNames.size());
                createInfo.ppEnabledLayerNames = instance->layerNames.data();
            } else {
                createInfo.enabledLayerCount = 0;
            }

#ifdef AE_OS_MACOS
            VkPhysicalDevicePortabilitySubsetFeaturesKHR portabilityFeatures = {};
            // This is hacked since I can't get it to work otherwise
            // See VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR in vulkan_core.h
            portabilityFeatures.sType = static_cast<VkStructureType>(1000163000);
            portabilityFeatures.mutableComparisonSamplers = VK_TRUE;
            portabilityFeatures.pNext = &features;

            // This feature struct is the last one in the pNext chain for now
            createInfo.pNext = &portabilityFeatures;
#else
            createInfo.pNext = &features;
#endif

            VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device))

            vkGetDeviceQueue(device, queueFamilyIndices.queueFamilies[QueueType::GraphicsQueue].value(), 0,
                &queueFamilyIndices.queues[QueueType::GraphicsQueue]);
            vkGetDeviceQueue(device, queueFamilyIndices.queueFamilies[QueueType::PresentationQueue].value(), 0,
                &queueFamilyIndices.queues[QueueType::PresentationQueue]);
            vkGetDeviceQueue(device, queueFamilyIndices.queueFamilies[QueueType::TransferQueue].value(), 0,
                &queueFamilyIndices.queues[QueueType::TransferQueue]);

            memoryManager = new MemoryManager(this,
                queueFamilyIndices.queueFamilies[QueueType::TransferQueue].value(),
                queueFamilyIndices.queues[QueueType::TransferQueue]);

            CreateFrameData();

            isComplete = true;

        }

        GraphicsDevice::~GraphicsDevice() {

            // Make sure that all commands were processed before
            // deleting resources
            WaitForIdle();

            delete swapChain;

            DestroyFrameData();

            for (auto commandList : commandLists) {
                delete commandList;
            }

            // Deleted pipelines might reference e.g. still active frame buffers,
            // so delete all of the memoryManager content before cleaning the rest
            memoryManager->DestroyAllImmediate();

            for (auto& pipelineRef : pipelines) {
                assert(pipelineRef.use_count() == 1 && "Pipeline wasn't deallocated or allocated wrongly");
                pipelineRef.reset();
            }

            for (auto& frameBufferRef : frameBuffers) {
                assert(frameBufferRef.use_count() == 1 && "Frame buffer wasn't deallocated or allocated wrongly");
                frameBufferRef.reset();
            }

            for (auto& renderPassRef : renderPasses) {
                assert(renderPassRef.use_count() == 1 && "Render pass wasn't deallocated or allocated wrongly");
                renderPassRef.reset();
            }

            for (auto& shaderRef : shaders) {
                assert(shaderRef.use_count() == 1 && "Shader wasn't deallocated or allocated wrongly");
                shaderRef.reset();
            }

            for (auto& bufferRef : buffers) {
                assert(bufferRef.use_count() == 1 && "Buffer wasn't deallocated or allocated wrongly");
                bufferRef.reset();
            }

            for (auto& multiBufferRef : multiBuffers) {
                assert(multiBufferRef.use_count() == 1 && "Multi buffer wasn't deallocated or allocated wrongly");
                multiBufferRef.reset();
            }

            for (auto& imageRef : images) {
                assert(imageRef.use_count() == 1 && "Image wasn't deallocated or allocated wrongly");
                imageRef.reset();
            }

            for (auto& samplerRef : samplers) {
                assert(samplerRef.use_count() == 1 && "Sampler wasn't deallocated or allocated wrongly");
                samplerRef.reset();
            }

            for (auto& poolRef : descriptorPools) {
                assert(poolRef.use_count() == 1 && "Descriptor pool wasn't deallocated or allocated wrongly");
                poolRef.reset();
            }

            for (auto& poolRef : queryPools) {
                assert(poolRef.use_count() == 1 && "Query pool wasn't deallocated or allocated wrongly");
                poolRef.reset();
            }

            // Delete memory manager at last, since it needs to clean up
            // all remaining memory which was destroyed by buffers, images, etc.
            delete memoryManager;
            vkDestroyDevice(device, nullptr);

        }

        SwapChain* GraphicsDevice::CreateSwapChain(VkPresentModeKHR presentMode) {

            auto nativeSurface = surface->GetNativeSurface();
            auto nativeWindow = surface->GetNativeWindow();

            auto supportDetails = SwapChainSupportDetails(physicalDevice, nativeSurface);

            int32_t width, height;
            SDL_GL_GetDrawableSize(nativeWindow, &width, &height);

            windowWidth = width;
            windowHeight = height;

            WaitForIdle();

            // Semaphores can be in an invalid state after swap chain recreation
            // To prevent this we also recreate the semaphores
            for (auto& frame : frameData) {
                frame.RecreateSemaphore(device);
            }

            // Had some issue with passing the old swap chain and then deleting it after x frames.
            delete swapChain;
            swapChain = new SwapChain(supportDetails, nativeSurface, this,
                windowWidth, windowHeight, presentMode, VK_NULL_HANDLE);

            // Acquire first index since normally these are acquired at completion of frame
            auto frame = GetFrameData();

            VkSemaphoreCreateInfo semaphoreInfo = Initializers::InitSemaphoreCreateInfo();
            vkDestroySemaphore(device, frame->semaphore, nullptr);
            VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frame->semaphore))

            swapChain->AcquireImageIndex(frame->semaphore);

            return swapChain;

        }

        Ref<RenderPass> GraphicsDevice::CreateRenderPass(RenderPassDesc desc) {

            auto renderPass = std::make_shared<RenderPass>(this, desc);

            renderPasses.push_back(renderPass);

            return renderPass;

        }

        Ref<FrameBuffer> GraphicsDevice::CreateFrameBuffer(FrameBufferDesc desc) {

            auto frameBuffer = std::make_shared<FrameBuffer>(this, desc);

            frameBuffers.push_back(frameBuffer);

            return frameBuffer;

        }

        Ref<Shader> GraphicsDevice::CreateShader(ShaderDesc desc) {

            auto shader = std::make_shared<Shader>(this, desc);

            shaders.push_back(shader);

            return shader;

        }

        Ref<Pipeline> GraphicsDevice::CreatePipeline(GraphicsPipelineDesc desc) {

            auto pipeline = std::make_shared<Pipeline>(this, desc);

            pipelines.push_back(pipeline);

            return pipeline;

        }

        Ref<Pipeline> GraphicsDevice::CreatePipeline(ComputePipelineDesc desc) {

            auto pipeline = std::make_shared<Pipeline>(this, desc);

            pipelines.push_back(pipeline);

            return pipeline;

        }

        Ref<Buffer> GraphicsDevice::CreateBuffer(BufferDesc desc) {

            auto buffer = std::make_shared<Buffer>(this, desc);

            buffers.push_back(buffer);

            return buffer;

        }

        Ref<MultiBuffer> GraphicsDevice::CreateMultiBuffer(BufferDesc desc) {

            auto multiBuffer = std::make_shared<MultiBuffer>(this, desc);

            multiBuffers.push_back(multiBuffer);

            return multiBuffer;

        }

        Ref<Image> GraphicsDevice::CreateImage(ImageDesc desc) {

            auto image = std::make_shared<Image>(this, desc);

            images.push_back(image);

            return image;

        }

        Ref<Sampler> GraphicsDevice::CreateSampler(SamplerDesc desc) {

            auto sampler = std::make_shared<Sampler>(this, desc);

            samplers.push_back(sampler);

            return sampler;

        }

        Ref<DescriptorPool> GraphicsDevice::CreateDescriptorPool() {

            auto pool = std::make_shared<DescriptorPool>(this);

            descriptorPools.push_back(pool);

            return pool;

        }

        Ref<QueryPool> GraphicsDevice::CreateQueryPool(QueryPoolDesc desc) {

            auto pool = std::make_shared<QueryPool>(this, desc);

            queryPools.push_back(pool);

            return pool;

        }

        CommandList* GraphicsDevice::GetCommandList(QueueType queueType, bool frameIndependentList) {

            if (frameIndependentList) {
                auto commandList = GetOrCreateCommandList(queueType, commandListsMutex,
                    commandLists, true);

                commandList->isSubmitted = false;
                commandList->dependencies.clear();

                return commandList;
            }
            else {
                auto currentFrameData = GetFrameData();

                auto commandList = GetOrCreateCommandList(queueType, currentFrameData->commandListsMutex,
                    currentFrameData->commandLists, false);

                commandList->isSubmitted = false;
                commandList->dependencies.clear();

                return commandList;
            }

        }

        void GraphicsDevice::SubmitCommandList(CommandList *cmd, VkPipelineStageFlags waitStage, ExecutionOrder order) {

            assert(!cmd->frameIndependent && "Submitted command list is frame independent."
                && "Please use the flush method instead");

            assert(swapChain->isComplete && "Swap chain should be complete."
                && " The swap chain might have an invalid size due to a window resize");

            // After the submission of a command list, we don't unlock it anymore
            // for further use in this frame. Instead, we will unlock it again
            // when we get back to this frames data and start a new frame with it.
            auto frame = GetFrameData();

            cmd->executionOrder = order;

            std::vector<VkPipelineStageFlags> waitStages = { waitStage };

            std::vector<VkSemaphore> waitSemaphores;
            std::vector<VkSemaphore> submitSemaphores;
            // Leave out any dependencies if the swap chain isn't complete
            if (swapChain->isComplete) {
                waitSemaphores = { frame->semaphore };
                submitSemaphores = { cmd->semaphore };
                if (frame->submittedCommandLists.size() > 0 && order == ExecutionOrder::Sequential) {
                    waitSemaphores[0] = frame->submittedCommandLists.back()->semaphore;
                }
                else if (order == ExecutionOrder::Parallel) {

                }
            }

            VkSubmitInfo submit = {};
            submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit.pNext = nullptr;
            submit.pWaitDstStageMask = waitStages.data();
            submit.waitSemaphoreCount = uint32_t(waitSemaphores.size());
            submit.pWaitSemaphores = waitSemaphores.data();
            submit.signalSemaphoreCount = uint32_t(submitSemaphores.size());
            submit.pSignalSemaphores = submitSemaphores.data();
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &cmd->commandBuffer;

            auto queue = queueFamilyIndices.queues[cmd->queueType];
            VK_CHECK(vkQueueSubmit(queue, 1, &submit, cmd->fence))

            // Make sure only one command list at a time can be added
            std::lock_guard<std::mutex> lock(frame->submissionMutex);

            frame->submittedCommandLists.push_back(cmd);
            cmd->isSubmitted = true;

        }

        void GraphicsDevice::FlushCommandList(CommandList *cmd) {

            assert(cmd->frameIndependent && "Flushed command list is not frame independent."
                   && "Please use the submit method instead");

            VkSubmitInfo submit = {};
            submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit.pNext = nullptr;
            submit.pWaitDstStageMask = nullptr;
            submit.waitSemaphoreCount = 0;
            submit.pWaitSemaphores = nullptr;
            submit.signalSemaphoreCount = 0;
            submit.pSignalSemaphores = nullptr;
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &cmd->commandBuffer;

            auto queue = queueFamilyIndices.queues[cmd->queueType];
            VK_CHECK(vkQueueSubmit(queue, 1, &submit, cmd->fence))

            VK_CHECK(vkWaitForFences(device, 1, &cmd->fence, true, 9999999999));
            VK_CHECK(vkResetFences(device, 1, &cmd->fence));

            // Is submitted now and must be unlocked
            cmd->isSubmitted = true;
            cmd->isLocked = false;

        }

        void GraphicsDevice::CompleteFrame() {

            bool recreateSwapChain = false;

            auto frame = GetFrameData();
            // Lock mutex such that submissions can't happen anymore
            std::lock_guard<std::mutex> lock(frame->submissionMutex);

            bool allListSubmitted = true;
            for (auto commandList : frame->commandLists) {
                allListSubmitted &= commandList->isSubmitted;
            }

            assert(allListSubmitted && "Not all command list were submitted before frame completion." &&
                "Consider using a frame independent command lists for longer executions.");

            if (frame->submittedCommandLists.size() && swapChain->isComplete) {
                std::vector<VkSemaphore> semaphores;
                // For now, we will only use sequential execution of queue submits,
                // which means only the latest submit can signal its semaphore here
                //for (auto cmd : frameData->submittedCommandLists)
                //    semaphores.push_back(cmd->semaphore);
                semaphores.push_back(frame->submittedCommandLists.back()->semaphore);

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
            }

            // Delete data that is marked for deletion for this frame
            memoryManager->DeleteData();
            frameIndex++;

            // Do before update anything, don't want to do unnecessary work
            DestroyUnusedGraphicObjects();

            // Update frame index of all objects in need
            memoryManager->UpdateFrameIndex(frameIndex);
            for (auto& multiBuffer : multiBuffers) {
                multiBuffer->UpdateFrameIndex(frameIndex);
            }

            auto nextFrame = GetFrameData();
            if (swapChain->AcquireImageIndex(nextFrame->semaphore)) {
                recreateSwapChain = true;
            }

            if (recreateSwapChain || CheckForWindowResize()) {
                // A new image index is automatically acquired
                CreateSwapChain();
            }

            // Wait, reset and start with new semaphores
            nextFrame->WaitAndReset(device);

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

        void GraphicsDevice::ForceMemoryCleanup() {

            // Do before update anything, don't want to do unnecessary work
            DestroyUnusedGraphicObjects();

            memoryManager->DestroyAllImmediate();

        }

        bool GraphicsDevice::SelectPhysicalDevice(VkInstance instance, VkSurfaceKHR surface,
            const std::vector<const char*>& requiredExtensions) {

            uint32_t deviceCount = 0;
            VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr));
            if (!deviceCount) return false;

            std::vector<VkPhysicalDevice> devices(deviceCount);
            VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()))

            std::multimap<int32_t, VkPhysicalDevice> candidates;
            for (const auto& candidate : devices) {
                int32_t score = RateDeviceSuitability(candidate, surface, requiredExtensions);
                candidates.insert(std::make_pair(score, candidate));
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

            VkPhysicalDeviceProperties physicalDeviceProperties;
            VkPhysicalDeviceFeatures physicalDeviceFeatures;
            vkGetPhysicalDeviceProperties(device, &physicalDeviceProperties);
            vkGetPhysicalDeviceFeatures(device, &physicalDeviceFeatures);

            // This property has to outweigh any other
            if (physicalDeviceProperties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                score += 10000;
            }

            // Maximum possible size of textures affects graphics quality
            score += physicalDeviceProperties.limits.maxImageDimension2D;

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

                if (!physicalDeviceFeatures.samplerAnisotropy) {
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

                if (queueFamilyIndices.IsComplete())
                    break;

                counter++;
            }

            if (!queueFamilyIndices.IsComplete()) return false;

            counter = 0;
            for (auto& queueFamily : queueFamilies) {
                // Try to find different queue for transfers
                if (counter == queueFamilyIndices.queueFamilies[QueueType::GraphicsQueue]) {
                    counter++;
                    continue;
                }

                if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) {
                    queueFamilyIndices.queueFamilies[QueueType::TransferQueue] = counter;
                    break;
                }

                counter++;
            }

            counter = 0;
            for (auto& queueFamily : queueFamilies) {
                // Try to find different queue for presentation
                if (counter == queueFamilyIndices.queueFamilies[QueueType::GraphicsQueue] ||
                    counter == queueFamilyIndices.queueFamilies[QueueType::TransferQueue]) {
                    counter++;
                    continue;
                }

                VkBool32 presentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, counter, surface, &presentSupport);
                if (presentSupport) {
                    queueFamilyIndices.queueFamilies[QueueType::PresentationQueue] = counter;
                    break;
                }

                counter++;
            }

            return true;

        }

        std::vector<VkDeviceQueueCreateInfo> GraphicsDevice::CreateQueueInfos(float* priority) {

            std::vector<VkDeviceQueueCreateInfo> queueInfos;
            std::set<uint32_t> queueFamilies = {
                    queueFamilyIndices.queueFamilies[QueueType::GraphicsQueue].value(),
                    queueFamilyIndices.queueFamilies[QueueType::PresentationQueue].value(),
                    queueFamilyIndices.queueFamilies[QueueType::TransferQueue].value(),
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

        void GraphicsDevice::BuildPhysicalDeviceFeatures(VkPhysicalDevice device) {

            // Initialize feature struct appropriately
            features = {};
            features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            features11 = {};
            features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
            features12 = {};
            features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

            // Point to the next features
            features.pNext = &features11;
            features11.pNext = &features12;

            // This queries all features in the chain
            vkGetPhysicalDeviceFeatures2(physicalDevice, &features);

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
            VkSemaphoreCreateInfo semaphoreInfo = Initializers::InitSemaphoreCreateInfo();
            
            for (int32_t i = 0; i < FRAME_DATA_COUNT; i++) {
                // Create secondary fences in a signaled state
                if (i > 0) fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
                VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &frameData[i].fence))
                VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frameData[i].semaphore))
            }

        }

        void GraphicsDevice::DestroyFrameData() {

            for (int32_t i = 0; i < FRAME_DATA_COUNT; i++) {
                auto& frameCommandLists = frameData[i].commandLists;
                for (auto commandList : frameCommandLists) {
                    delete commandList;
                }

                vkDestroyFence(device, frameData[i].fence, nullptr);
                vkDestroySemaphore(device, frameData[i].semaphore, nullptr);
            }

        }

        FrameData *GraphicsDevice::GetFrameData() {

            return &frameData[frameIndex % FRAME_DATA_COUNT];

        }

        void GraphicsDevice::DestroyUnusedGraphicObjects() {

            for (size_t i = 0; i < renderPasses.size(); i++) {
                auto& renderPassRef = renderPasses[i];
                if (renderPassRef.use_count() == 1) {
                    renderPassRef.swap(renderPasses.back());
                    memoryManager->DestroyAllocation(renderPasses.back());
                    renderPasses.pop_back();
                    i--;
                }
            }

            for (size_t i = 0; i < pipelines.size(); i++) {
                auto& pipelineRef = pipelines[i];
                if (pipelineRef.use_count() == 1) {
                    pipelineRef.swap(pipelines.back());
                    memoryManager->DestroyAllocation(pipelines.back());
                    pipelines.pop_back();
                    i--;
                }
            }

            for (size_t i = 0; i < shaders.size(); i++) {
                auto& shaderRef = shaders[i];
                if (shaderRef.use_count() == 1) {
                    shaderRef.swap(shaders.back());
                    memoryManager->DestroyAllocation(shaders.back());
                    shaders.pop_back();
                    i--;
                }
            }

            for (size_t i = 0; i < buffers.size(); i++) {
                auto& bufferRef = buffers[i];
                if (bufferRef.use_count() == 1) {
                    bufferRef.swap(buffers.back());
                    memoryManager->DestroyAllocation(buffers.back());
                    buffers.pop_back();
                    i--;
                }
            }

            for (size_t i = 0; i < multiBuffers.size(); i++) {
                auto& multiBufferRef = multiBuffers[i];
                if (multiBufferRef.use_count() == 1) {
                    multiBufferRef.swap(multiBuffers.back());
                    memoryManager->DestroyAllocation(multiBuffers.back());
                    multiBuffers.pop_back();
                    i--;
                }
            }

            for (size_t i = 0; i < images.size(); i++) {
                auto& imageRef = images[i];
                if (imageRef.use_count() == 1) {
                    imageRef.swap(images.back());
                    memoryManager->DestroyAllocation(images.back());
                    images.pop_back();
                    i--;
                }
            }

            for (size_t i = 0; i < samplers.size(); i++) {
                auto& samplerRef = samplers[i];
                if (samplerRef.use_count() == 1) {
                    samplerRef.swap(samplers.back());
                    memoryManager->DestroyAllocation(samplers.back());
                    samplers.pop_back();
                    i--;
                }
            }

            for (size_t i = 0; i < descriptorPools.size(); i++) {
                auto& poolRef = descriptorPools[i];
                if (poolRef.use_count() == 1) {
                    poolRef.swap(descriptorPools.back());
                    memoryManager->DestroyAllocation(descriptorPools.back());
                    descriptorPools.pop_back();
                    i--;
                }
            }

            for (size_t i = 0; i < queryPools.size(); i++) {
                auto& poolRef = queryPools[i];
                if (poolRef.use_count() == 1) {
                    poolRef.swap(queryPools.back());
                    memoryManager->DestroyAllocation(queryPools.back());
                    queryPools.pop_back();
                    i--;
                }
            }

        }

        CommandList* GraphicsDevice::GetOrCreateCommandList(QueueType queueType, std::mutex &mutex,
            std::vector<CommandList*> &cmdLists, bool frameIndependent) {

            std::lock_guard<std::mutex> lock(mutex);

            auto it = std::find_if(cmdLists.begin(), cmdLists.end(),
                [&](CommandList *commandList) {
                    // Second condition condition should be given by default
                    if (commandList->queueType == queueType &&
                        commandList->frameIndependent == frameIndependent) {
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

            if (it == cmdLists.end()) {
                auto queueFamilyIndex = queueFamilyIndices.queueFamilies[queueType];
                CommandList *cmd = new CommandList(this, queueType,
                    queueFamilyIndex.value(), frameIndependent);
                cmdLists.push_back(cmd);
                return cmd;
            }

            return *it;

        }

    }

}
