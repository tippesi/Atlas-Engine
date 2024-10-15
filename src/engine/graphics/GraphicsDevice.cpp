#include "GraphicsDevice.h"
#include "Instance.h"
#include "StructureChainBuilder.h"
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

            std::vector<const char*> requiredExtensions = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            };

            std::vector<const char*> optionalExtensions = {
                VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
                VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
                VK_KHR_RAY_QUERY_EXTENSION_NAME,
                VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
                VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME,
                VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME,
                VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME,
                VK_NV_RAY_TRACING_VALIDATION_EXTENSION_NAME
#ifdef AE_BINDLESS
                , VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
#endif
#ifdef AE_BUILDTYPE_DEBUG
                , VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME
#endif
#ifdef AE_OS_MACOS
                , VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
#endif
            };

            SelectPhysicalDevice(instance->instance, surface->GetNativeSurface(),
                requiredExtensions, optionalExtensions);

            auto availableOptionalExtension = CheckDeviceOptionalExtensionSupport(physicalDevice, optionalExtensions);
            requiredExtensions.insert(requiredExtensions.end(), availableOptionalExtension.begin(),
                availableOptionalExtension.end());

            GetPhysicalDeviceProperties(physicalDevice);            

            auto queueCreateInfos = CreateQueueInfos();

            BuildPhysicalDeviceFeatures(physicalDevice);

            // Uses the physical device structures generated above
            CreateDevice(queueCreateInfos, requiredExtensions, enableValidationLayers);

            for (auto& queueFamily : queueFamilyIndices.families) {
                for (auto& queue : queueFamily.queues) {
                    vkGetDeviceQueue(device, queue->familyIndex, queue->index, &queue->queue);
                }
            }

            memoryManager = new MemoryManager(this);

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

            // We assume everything else is terminated by now, so this is the only thread still alive
            // In that case we don't lock all the mutexes
            for (auto& tlasRef : tlases.data) {
                AE_ASSERT(tlasRef.use_count() == 1 && "TLAS wasn't deallocated or allocated wrongly");
                tlasRef.reset();
            }

            for (auto& blasRef : blases.data) {
                AE_ASSERT(blasRef.use_count() == 1 && "BLAS wasn't deallocated or allocated wrongly");
                blasRef.reset();
            }

            for (auto& pipelineRef : pipelines.data) {
                AE_ASSERT(pipelineRef.use_count() == 1 && "Pipeline wasn't deallocated or allocated wrongly");
                pipelineRef.reset();
            }

            for (auto& frameBufferRef : frameBuffers.data) {
                AE_ASSERT(frameBufferRef.use_count() == 1 && "Frame buffer wasn't deallocated or allocated wrongly");
                frameBufferRef.reset();
            }

            for (auto& renderPassRef : renderPasses.data) {
                AE_ASSERT(renderPassRef.use_count() == 1 && "Render pass wasn't deallocated or allocated wrongly");
                renderPassRef.reset();
            }

            for (auto& shaderRef : shaders.data) {
                AE_ASSERT(shaderRef.use_count() == 1 && "Shader wasn't deallocated or allocated wrongly");
                shaderRef.reset();
            }

            for (auto& bufferRef : buffers.data) {
                AE_ASSERT(bufferRef.use_count() == 1 && "Buffer wasn't deallocated or allocated wrongly");
                bufferRef.reset();
            }

            for (auto& multiBufferRef : multiBuffers.data) {
                AE_ASSERT(multiBufferRef.use_count() == 1 && "Multi buffer wasn't deallocated or allocated wrongly");
                multiBufferRef.reset();
            }

            for (auto& imageRef : images.data) {
                AE_ASSERT(imageRef.use_count() == 1 && "Image wasn't deallocated or allocated wrongly");
                imageRef.reset();
            }

            for (auto& samplerRef : samplers.data) {
                AE_ASSERT(samplerRef.use_count() == 1 && "Sampler wasn't deallocated or allocated wrongly");
                samplerRef.reset();
            }

            for (auto& poolRef : descriptorPools.data) {
                AE_ASSERT(poolRef.use_count() == 1 && "Descriptor pool wasn't deallocated or allocated wrongly");
                poolRef.reset();
            }

            for (auto& descLayoutRef : descriptorSetLayouts.data) {
                AE_ASSERT(descLayoutRef.use_count() == 1 && "Descriptor layout wasn't deallocated or allocated wrongly");
                descLayoutRef.reset();
            }

            for (auto& poolRef : queryPools.data) {
                AE_ASSERT(poolRef.use_count() == 1 && "Query pool wasn't deallocated or allocated wrongly");
                poolRef.reset();
            }

            // Delete memory manager at last, since it needs to clean up
            // all remaining memory which was destroyed by buffers, images, etc.
            delete memoryManager;
            vkDestroyDevice(device, nullptr);

        }

        SwapChain* GraphicsDevice::CreateSwapChain(VkPresentModeKHR presentMode, ColorSpace preferredColorSpace) {

            auto nativeSurface = surface->GetNativeSurface();

            auto supportDetails = SwapChainSupportDetails(physicalDevice, nativeSurface);

            int32_t width, height;
            surface->GetExtent(width, height);

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
                windowWidth, windowHeight, preferredColorSpace, presentMode, VK_NULL_HANDLE);

            // Acquire first index since normally these are acquired at completion of frame
            auto frame = GetFrameData(frameIndex);

            VkSemaphoreCreateInfo semaphoreInfo = Initializers::InitSemaphoreCreateInfo();
            vkDestroySemaphore(device, frame->semaphore, nullptr);
            VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frame->semaphore))

            swapChain->AcquireImageIndex(frame->semaphore);

            return swapChain;

        }

        Ref<RenderPass> GraphicsDevice::CreateRenderPass(RenderPassDesc desc) {

            auto renderPass = std::make_shared<RenderPass>(this, desc);

            std::lock_guard<std::mutex> guard(renderPasses.mutex);
            renderPasses.data.push_back(renderPass);

            return renderPass;

        }

        Ref<FrameBuffer> GraphicsDevice::CreateFrameBuffer(FrameBufferDesc desc) {

            auto frameBuffer = std::make_shared<FrameBuffer>(this, desc);

            std::lock_guard<std::mutex> guard(frameBuffers.mutex);
            frameBuffers.data.push_back(frameBuffer);

            return frameBuffer;

        }

        Ref<Shader> GraphicsDevice::CreateShader(ShaderDesc desc) {

            auto shader = std::make_shared<Shader>(this, desc);

            std::lock_guard<std::mutex> guard(shaders.mutex);
            shaders.data.push_back(shader);

            return shader;

        }

        Ref<Pipeline> GraphicsDevice::CreatePipeline(GraphicsPipelineDesc desc) {

            auto pipeline = std::make_shared<Pipeline>(this, desc);

            std::lock_guard<std::mutex> guard(pipelines.mutex);
            pipelines.data.push_back(pipeline);

            return pipeline;

        }

        Ref<Pipeline> GraphicsDevice::CreatePipeline(ComputePipelineDesc desc) {

            auto pipeline = std::make_shared<Pipeline>(this, desc);

            std::lock_guard<std::mutex> guard(pipelines.mutex);
            pipelines.data.push_back(pipeline);

            return pipeline;

        }

        Ref<Buffer> GraphicsDevice::CreateBuffer(BufferDesc desc) {

            auto buffer = std::make_shared<Buffer>(this, desc);

            std::lock_guard<std::mutex> guard(buffers.mutex);
            buffers.data.push_back(buffer);

            return buffer;

        }

        Ref<MultiBuffer> GraphicsDevice::CreateMultiBuffer(BufferDesc desc) {

            auto multiBuffer = std::make_shared<MultiBuffer>(this, desc);

            std::lock_guard<std::mutex> guard(multiBuffers.mutex);
            multiBuffers.data.push_back(multiBuffer);

            return multiBuffer;

        }

        Ref<Image> GraphicsDevice::CreateImage(ImageDesc desc) {

            auto image = std::make_shared<Image>(this, desc);

            std::lock_guard<std::mutex> guard(images.mutex);
            images.data.push_back(image);

            return image;

        }

        Ref<Sampler> GraphicsDevice::CreateSampler(SamplerDesc desc) {

            auto sampler = std::make_shared<Sampler>(this, desc);

            std::lock_guard<std::mutex> guard(samplers.mutex);
            samplers.data.push_back(sampler);

            return sampler;

        }

        Ref<DescriptorSetLayout> GraphicsDevice::CreateDescriptorSetLayout(DescriptorSetLayoutDesc desc) {

            auto layout = std::make_shared<DescriptorSetLayout>(this, desc);

            std::lock_guard<std::mutex> guard(descriptorSetLayouts.mutex);
            descriptorSetLayouts.data.push_back(layout);

            return layout;

        }

        Ref<DescriptorPool> GraphicsDevice::CreateDescriptorPool(DescriptorPoolDesc desc) {

            auto pool = std::make_shared<DescriptorPool>(this, desc);

            std::lock_guard<std::mutex> guard(descriptorPools.mutex);
            descriptorPools.data.push_back(pool);

            return pool;

        }

        Ref<QueryPool> GraphicsDevice::CreateQueryPool(QueryPoolDesc desc) {

            auto pool = std::make_shared<QueryPool>(this, desc);

            std::lock_guard<std::mutex> guard(queryPools.mutex);
            queryPools.data.push_back(pool);

            return pool;

        }

        Ref<BLAS> GraphicsDevice::CreateBLAS(BLASDesc desc) {

            auto blas = std::make_shared<BLAS>(this, desc);

            std::lock_guard<std::mutex> guard(blases.mutex);
            blases.data.push_back(blas);

            return blas;

        }

        Ref<TLAS> GraphicsDevice::CreateTLAS(TLASDesc desc) {

            auto tlas = std::make_shared<TLAS>(this, desc);

            std::lock_guard<std::mutex> guard(tlases.mutex);
            tlases.data.push_back(tlas);

            return tlas;

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
                auto currentFrameData = GetFrameData(frameIndex);

                auto commandList = GetOrCreateCommandList(queueType, currentFrameData->commandListsMutex,
                    currentFrameData->commandLists, false);

                commandList->isSubmitted = false;
                commandList->dependencies.clear();

                return commandList;
            }

        }

        void GraphicsDevice::SubmitCommandList(CommandList *cmd, VkPipelineStageFlags waitStage, ExecutionOrder order) {

            AE_ASSERT(!cmd->frameIndependent && "Submitted command list is frame independent."
                && "Please use the flush method instead");

            AE_ASSERT(swapChain->isComplete && "Swap chain should be complete."
                && " The swap chain might have an invalid size due to a window resize");

            AE_ASSERT(!cmd->isSubmitted && "Command list was probably submitted before");

            auto frame = GetFrameData(frameIndex);

            cmd->executionOrder = order;

            // Make sure only one command list at a time can be added
            std::lock_guard<std::mutex> lock(frame->submissionMutex);

            CommandListSubmission submission = {
                .cmd = cmd,
                .waitStage = waitStage
            };

            frame->submissions.push_back(submission);
            frame->submittedCommandLists.push_back(cmd);
            cmd->isSubmitted = true;

        }

        void GraphicsDevice::FlushCommandList(CommandList *cmd) {

            std::shared_lock lock(queueMutex);

            AE_ASSERT(cmd->frameIndependent && "Flushed command list is not frame independent."
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

            auto queue = FindAndLockQueue(cmd->queueFamilyIndex);
            VK_CHECK(vkQueueSubmit(queue.queue, 1, &submit, cmd->fence))
            queue.Unlock();

            VK_CHECK(vkWaitForFences(device, 1, &cmd->fence, true, 30000000000));
            VK_CHECK(vkResetFences(device, 1, &cmd->fence));

            // Is submitted now and must be unlocked
            cmd->isSubmitted = true;
            cmd->isLocked = false;

        }

        bool GraphicsDevice::IsPreviousFrameSubmitted() {

            return frameSubmissionComplete;

        }

        void GraphicsDevice::WaitForPreviousFrameSubmission() {

            JobSystem::Wait(submitFrameJob);

        }

        void GraphicsDevice::SubmitFrameAsync() {

            frameSubmissionComplete = false;

            JobSystem::Wait(submitFrameJob);

            // Recreate a swapchain on with MoltenVK seems to not work (just never returns)
            // Refrain from using complete frame async for now
            JobSystem::Execute(submitFrameJob, [this] (JobData&) { SubmitFrame(); });

        }

        void GraphicsDevice::SubmitFrame() {

            bool recreateSwapChain = false;

            auto frame = GetFrameData(frameIndex);

            bool wasSwapChainAccessed = false;
            for (auto commandList : frame->submittedCommandLists) {
                wasSwapChainAccessed |= commandList->wasSwapChainAccessed;
            }

            // Do dummy commandList submit in case swap chain was not accessed in current frame
            if (!wasSwapChainAccessed && swapChain->isComplete) {
                auto commandList = GetCommandList(GraphicsQueue);
                commandList->BeginCommands();
                commandList->BeginRenderPass(swapChain, true);
                commandList->EndRenderPass();
                commandList->EndCommands();
                SubmitCommandList(commandList);
            }

            // Lock mutex such that submissions can't happen anymore
            std::lock_guard<std::mutex> frameSubmissionLock(frame->submissionMutex);

            bool allListSubmitted = true;
            for (auto commandList : frame->commandLists) {
                allListSubmitted &= commandList->isSubmitted;
            }

            AE_ASSERT(allListSubmitted && "Not all command list were submitted before frame completion." &&
                "Consider using a frame independent command lists for longer executions.");

            // Wait, reset and start with new semaphores
            auto nextFrame = GetFrameData(frameIndex + 1);
            nextFrame->WaitAndReset(device);

            {
                // Lock the queue in shared mode such that flush can happen simultaneously, but waiting for idle is not possible
                // Not locking has caused some trouble, with WaitForIdle seeming to access queues if not guarded properly
                std::shared_lock queueLock(queueMutex);
                auto presenterQueue = SubmitAllCommandLists();

                if (swapChain->isComplete && frame->submittedCommandLists.size()) {

                    std::vector<VkSemaphore> semaphores;
                    // For now, we will only use sequential execution of queue submits,
                    // which means only the latest submit can signal its semaphore here
                    //for (auto cmd : frameData->submittedCommandLists)
                    //    semaphores.push_back(cmd->semaphore);
                    if (frame->submittedCommandLists.size()) {
                        semaphores.push_back(frame->submittedCommandLists.back()->GetSemaphore(presenterQueue.queue));
                    }
                    else {
                        semaphores.push_back(frame->semaphore);
                    }

                    // Since there needs to be at least one semaphore, we can use it like this
                    frame->submitSemaphore = semaphores.back();

                    VkPresentInfoKHR presentInfo = {};
                    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
                    presentInfo.pNext = nullptr;
                    presentInfo.pSwapchains = &swapChain->swapChain;
                    presentInfo.swapchainCount = 1;
                    presentInfo.pWaitSemaphores = semaphores.data();
                    presentInfo.waitSemaphoreCount = uint32_t(semaphores.size());
                    presentInfo.pImageIndices = &swapChain->aquiredImageIndex;

                    auto result = vkQueuePresentKHR(presenterQueue.queue, &presentInfo);
                    presenterQueue.Unlock();

                    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
                        recreateSwapChain = true;
                    }
                    else {
                        VK_CHECK(result)
                    }
                }
            }

            // Delete data that is marked for deletion for this frame
            memoryManager->DeleteData();
            frameIndex++;

            // Do before update anything, don't want to do unnecessary work
            DestroyUnusedGraphicObjects();

            // Update frame index of all objects in need
            memoryManager->UpdateFrameIndex(frameIndex);

            {
                std::lock_guard<std::mutex> guard(multiBuffers.mutex);
                for (auto& multiBuffer : multiBuffers.data) {
                    multiBuffer->UpdateFrameIndex(frameIndex);
                }
            }

            if (swapChain->AcquireImageIndex(nextFrame->semaphore)) {
                recreateSwapChain = true;
            }

            if (recreateSwapChain || CheckForWindowResize() || !swapChain->isComplete) {
                // A new image index is automatically acquired
                CreateSwapChain(swapChain->presentMode, swapChain->colorSpace);
            }

            frameSubmissionComplete = true;

        }

        bool GraphicsDevice::CheckFormatSupport(VkFormat format, VkFormatFeatureFlags featureFlags) {

            VkFormatProperties properties;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
            return (properties.optimalTilingFeatures & featureFlags) == featureFlags;

        }

        QueueRef GraphicsDevice::GetAndLockQueue(Atlas::Graphics::QueueType queueType) {

            return FindAndLockQueue(queueType);

        }

        void GraphicsDevice::WaitForIdle() {

            WaitForPreviousFrameSubmission();

            std::unique_lock lock(queueMutex);

            vkDeviceWaitIdle(device);

        }

        void GraphicsDevice::ForceMemoryCleanup() {

            // Do before update anything, don't want to do unnecessary work
            DestroyUnusedGraphicObjects();

            memoryManager->DestroyAllImmediate();

        }

        void GraphicsDevice::SetDebugObjectName(const std::string& name, VkObjectType objectType, uint64_t handle) {

            if (!support.debugMarker)
                return;

            VkDebugUtilsObjectNameInfoEXT nameInfo = {};
            nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
            nameInfo.objectType = objectType;
            nameInfo.objectHandle = handle;
            nameInfo.pObjectName = name.c_str();
            vkSetDebugUtilsObjectNameEXT(device, &nameInfo);

        }

        QueueRef GraphicsDevice::SubmitAllCommandLists() {

            auto frame = GetFrameData(frameIndex);
            auto previousFrame = frameIndex != 0 ? GetFrameData(frameIndex - 1) : nullptr;

            // Assume all submission are in order
            QueueRef nextQueue;
            QueueRef queue;
            VkSemaphore previousSemaphore = frame->semaphore;
            VkSemaphore previousFrameSemaphore = previousFrame != nullptr ? 
                previousFrame->submitSemaphore : VK_NULL_HANDLE;
            for (size_t i = 0; i < frame->submissions.size(); i++) {
                auto submission = &frame->submissions[i];
                auto nextSubmission = i + 1 < frame->submissions.size() ? &frame->submissions[i + 1] : nullptr;

                auto familyIndex = submission->cmd->queueFamilyIndex;

                if (!queue.valid) {
                    queue = FindAndLockQueue(familyIndex);
                }
                if (nextSubmission != nullptr && queue.familyIndex != nextSubmission->cmd->queueFamilyIndex) {
                    nextQueue = FindAndLockQueue(nextSubmission->cmd->queueFamilyIndex);
                }
                else {
                    nextQueue = queue;
                }
                if (nextSubmission == nullptr) {
                    nextQueue = FindAndLockQueue(QueueType::PresentationQueue);
                }

                SubmitCommandList(submission, previousSemaphore, previousFrameSemaphore, queue, nextQueue);
                previousSemaphore = submission->cmd->GetSemaphore(nextQueue.queue);

                if (nextQueue.ref != queue.ref) {
                    queue.Unlock();
                }

                queue = nextQueue;
                // Only use this once
                previousFrameSemaphore = VK_NULL_HANDLE;
            }

            // Make sure to return the presentation queue
            if (!frame->submissions.size()) {
                queue = FindAndLockQueue(QueueType::PresentationQueue);
            }

            return queue;
        }

        void GraphicsDevice::SubmitCommandList(CommandListSubmission* submission, VkSemaphore previousSemaphore,
            VkSemaphore previousFrameSemaphore, const QueueRef& queue, const QueueRef& nextQueue) {

            // After the submission of a command list, we don't unlock it anymore
            // for further use in this frame. Instead, we will unlock it again
            // when we get back to this frames data and start a new frame with it.
            auto cmd = submission->cmd;
            std::vector<VkPipelineStageFlags> waitStages = { submission->waitStage };

            std::vector<VkSemaphore> waitSemaphores;
            std::vector<VkSemaphore> submitSemaphores = { cmd->GetSemaphore(nextQueue.queue) };

            // Leave out any dependencies if the swap chain isn't complete
            if (swapChain->isComplete) {
                waitSemaphores = { previousSemaphore };
                //if (previousFrameSemaphore != VK_NULL_HANDLE)
                //    waitSemaphores.push_back(previousFrameSemaphore);
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

            VK_CHECK(vkQueueSubmit(queue.queue, 1, &submit, cmd->fence))
        }

        bool GraphicsDevice::SelectPhysicalDevice(VkInstance instance, VkSurfaceKHR surface,
            const std::vector<const char*>& requiredExtensions, std::vector<const char*>& optionalExtensions) {

            uint32_t deviceCount = 0;
            VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr));
            if (!deviceCount) return false;

            std::vector<VkPhysicalDevice> devices(deviceCount);
            VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()))

            std::multimap<int32_t, VkPhysicalDevice> candidates;
            for (const auto& candidate : devices) {
                int32_t score = RateDeviceSuitability(candidate, surface, requiredExtensions, optionalExtensions);
                candidates.insert(std::make_pair(score, candidate));
            }

            auto foundSuitableDevice = candidates.rbegin()->first > 0;
            AE_ASSERT(foundSuitableDevice && "No suitable device found");
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
                AE_ASSERT(completeIndices && "No valid queue family found");
                if (!completeIndices) {
                    return false;
                }

                auto extensionsSupported = CheckDeviceExtensionSupport(physicalDevice, requiredExtensions);
                AE_ASSERT(extensionsSupported && "Some required extensions are not supported");
                if (!extensionsSupported) {
                    return false;
                }

            }

            return true;

        }

        int32_t GraphicsDevice::RateDeviceSuitability(VkPhysicalDevice device, VkSurfaceKHR surface,
            const std::vector<const char*>& requiredExtensions, std::vector<const char*>& optionalExtensions) {

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

            queueFamilyIndices.families.resize(queueFamilies.size());

            // We have to have unique queue objects in the end for the mutexes to work,
            // so create them all here and do the selection later
            uint32_t counter = 0;
            for (auto& queueFamily : queueFamilies) {
                VkBool32 presentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, counter, surface, &presentSupport);

                bool supportsGraphics = true;
                bool supportsTransfer = true;
                if (!(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
                    supportsGraphics = false;

                if (!(queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT))
                    supportsGraphics = false;

                if (!(queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT))
                    supportsTransfer = false;

                queueFamilyIndices.families[counter].queues.resize(size_t(queueFamily.queueCount));
                queueFamilyIndices.families[counter].queuePriorities.resize(size_t(queueFamily.queueCount));

                queueFamilyIndices.families[counter].index = counter;
                queueFamilyIndices.families[counter].supportsGraphics = supportsGraphics;
                queueFamilyIndices.families[counter].supportsTransfer = supportsTransfer;
                queueFamilyIndices.families[counter].supportsPresentation = presentSupport;
                
                for (uint32_t i = 0; i < queueFamily.queueCount; i++) {
                    auto queue = std::make_shared<Queue>();

                    queue->familyIndex = counter;
                    queue->index = i;

                    queue->supportsGraphics = supportsGraphics;
                    queue->supportsTransfer = supportsTransfer;
                    queue->supportsPresentation = presentSupport;

                    queueFamilyIndices.families[counter].queues[i] = queue;
                    queueFamilyIndices.families[counter].queuePriorities[i] = 1.0f;
                }

                counter++;
            }

            counter = 0;
            for (auto& queueFamily : queueFamilyIndices.families) {
                

                if (queueFamily.supportsGraphics) {
                    queueFamilyIndices.queueFamilies[QueueType::GraphicsQueue] = counter;
                }
                if (queueFamily.supportsTransfer) {
                    queueFamilyIndices.queueFamilies[QueueType::TransferQueue] = counter;
                }
                if (queueFamily.supportsPresentation) {
                    queueFamilyIndices.queueFamilies[QueueType::PresentationQueue] = counter;
                }

                if (queueFamilyIndices.IsComplete())
                    break;

                counter++;
            }

            if (!queueFamilyIndices.IsComplete()) return false;

            counter = 0;
            for (auto& queueFamily : queueFamilyIndices.families) {
                // Try to find different queue for transfers
                if (counter == queueFamilyIndices.queueFamilies[QueueType::GraphicsQueue]) {
                    counter++;
                    continue;
                }

                if (queueFamily.supportsTransfer) {
                    queueFamilyIndices.queueFamilies[QueueType::TransferQueue] = counter;
                    break;
                }

                counter++;
            }

            counter = 0;
            for (auto& queueFamily : queueFamilyIndices.families) {
                // Try to find different queue for presentation
                if (counter == queueFamilyIndices.queueFamilies[QueueType::GraphicsQueue] ||
                    counter == queueFamilyIndices.queueFamilies[QueueType::TransferQueue]) {
                    counter++;
                    continue;
                }

                if (queueFamily.supportsPresentation) {
                    queueFamilyIndices.queueFamilies[QueueType::PresentationQueue] = counter;
                    break;
                }

                counter++;
            }

            return true;

        }

        std::vector<VkDeviceQueueCreateInfo> GraphicsDevice::CreateQueueInfos() {

            std::vector<VkDeviceQueueCreateInfo> queueInfos;

            for (auto& queueFamily : queueFamilyIndices.families) {
                VkDeviceQueueCreateInfo queueCreateInfo{};
                queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueCreateInfo.queueFamilyIndex = queueFamily.index;
                queueCreateInfo.queueCount = uint32_t(queueFamily.queues.size());
                queueCreateInfo.pQueuePriorities = queueFamily.queuePriorities.data();
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

            return requiredExtensions.empty();

        }

        std::vector<const char*> GraphicsDevice::CheckDeviceOptionalExtensionSupport(VkPhysicalDevice physicalDevice,
            std::vector<const char*> &extensionNames) {

            uint32_t extensionCount;
            vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

            std::vector<VkExtensionProperties> availableExtensions(extensionCount);
            vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

            std::vector<const char*> extensionOverlap;
            for (const auto extensionName : extensionNames) {
                for (const auto& extension : availableExtensions) {
                    supportedExtensions.insert(extension.extensionName);

                    if (std::string(extension.extensionName) == std::string(extensionName)) {
                        extensionOverlap.push_back(extensionName);
                    }
                }
            }

            return extensionOverlap;

        }

        void GraphicsDevice::BuildPhysicalDeviceFeatures(VkPhysicalDevice device) {

            // Initialize feature struct appropriately
            availableFeatures = {};
            availableFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            availableFeatures11 = {};
            availableFeatures11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
            availableFeatures12 = {};
            availableFeatures12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

            // Point to the next features
            availableFeatures.pNext = &availableFeatures11;
            availableFeatures11.pNext = &availableFeatures12;

            // This queries all features in the chain
            vkGetPhysicalDeviceFeatures2(physicalDevice, &availableFeatures);

        }

        void GraphicsDevice::GetPhysicalDeviceProperties(VkPhysicalDevice device) {

            StructureChainBuilder propertiesBuilder(deviceProperties);

            accelerationStructureProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
            rayTracingPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
            subgroupSizeControlProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES;

            deviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            deviceProperties11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
            deviceProperties12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;

            propertiesBuilder.Append(deviceProperties11);
            propertiesBuilder.Append(deviceProperties12);

            if (supportedExtensions.contains(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME))
                propertiesBuilder.Append(rayTracingPipelineProperties);
            if (supportedExtensions.contains(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME))
                propertiesBuilder.Append(accelerationStructureProperties);
            if (supportedExtensions.contains(VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME))
                propertiesBuilder.Append(subgroupSizeControlProperties);

            vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProperties);

        }

        void GraphicsDevice::CreateDevice(const std::vector<VkDeviceQueueCreateInfo>& queueCreateInfos,
            const std::vector<const char*>& extensions, bool enableValidationLayers) {

            VkDeviceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            createInfo.pQueueCreateInfos = queueCreateInfos.data();
            createInfo.queueCreateInfoCount = uint32_t(queueCreateInfos.size());

            createInfo.pEnabledFeatures = nullptr;
            createInfo.enabledExtensionCount = uint32_t(extensions.size());
            createInfo.ppEnabledExtensionNames = extensions.data();

            StructureChainBuilder featureBuilder(createInfo);

            VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeature = {};
            accelerationStructureFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;

            VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeature = {};
            rtPipelineFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;

            VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeature = {};
            rayQueryFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;

            VkPhysicalDeviceMemoryPriorityFeaturesEXT memoryPriorityFeature = {};
            memoryPriorityFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT;

            VkPhysicalDeviceRayTracingValidationFeaturesNV validationFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_VALIDATION_FEATURES_NV };

            VkPhysicalDeviceFeatures2 features = {};
            VkPhysicalDeviceVulkan11Features features11 = {};
            VkPhysicalDeviceVulkan12Features features12 = {};
            features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
            features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

            features.features.tessellationShader = availableFeatures.features.tessellationShader;
            features.features.multiDrawIndirect = availableFeatures.features.tessellationShader;
            features.features.depthBounds = availableFeatures.features.depthBounds;
            features.features.wideLines = availableFeatures.features.wideLines;
            features.features.samplerAnisotropy = availableFeatures.features.samplerAnisotropy;
            features.features.shaderUniformBufferArrayDynamicIndexing = availableFeatures.features.shaderUniformBufferArrayDynamicIndexing;
            features.features.shaderSampledImageArrayDynamicIndexing = availableFeatures.features.shaderSampledImageArrayDynamicIndexing;
            features.features.shaderStorageBufferArrayDynamicIndexing = availableFeatures.features.shaderStorageBufferArrayDynamicIndexing;
            features.features.shaderStorageImageArrayDynamicIndexing = availableFeatures.features.shaderStorageImageArrayDynamicIndexing;
            features.features.shaderStorageImageWriteWithoutFormat = availableFeatures.features.shaderStorageImageWriteWithoutFormat;
            features.features.shaderFloat64 = availableFeatures.features.shaderFloat64;
            features.features.shaderInt64 = availableFeatures.features.shaderInt64;
            features.features.shaderInt16 = availableFeatures.features.shaderInt16;
            features.features.independentBlend = availableFeatures.features.independentBlend;

            features12.descriptorIndexing = availableFeatures12.descriptorIndexing;
            features12.shaderUniformBufferArrayNonUniformIndexing = availableFeatures12.shaderUniformBufferArrayNonUniformIndexing;
            features12.shaderSampledImageArrayNonUniformIndexing = availableFeatures12.shaderSampledImageArrayNonUniformIndexing;
            features12.shaderStorageBufferArrayNonUniformIndexing = availableFeatures12.shaderStorageBufferArrayNonUniformIndexing;
            features12.shaderStorageImageArrayNonUniformIndexing = availableFeatures12.shaderStorageImageArrayNonUniformIndexing;
            features12.descriptorBindingUniformBufferUpdateAfterBind = availableFeatures12.descriptorBindingUniformBufferUpdateAfterBind;
            features12.descriptorBindingSampledImageUpdateAfterBind = availableFeatures12.descriptorBindingSampledImageUpdateAfterBind;
            features12.descriptorBindingStorageImageUpdateAfterBind = availableFeatures12.descriptorBindingStorageImageUpdateAfterBind;
            features12.descriptorBindingStorageBufferUpdateAfterBind = availableFeatures12.descriptorBindingStorageBufferUpdateAfterBind;
            features12.descriptorBindingPartiallyBound = availableFeatures12.descriptorBindingPartiallyBound;
            features12.descriptorBindingVariableDescriptorCount = availableFeatures12.descriptorBindingVariableDescriptorCount;
            features12.hostQueryReset = availableFeatures12.hostQueryReset;
            features12.bufferDeviceAddress = availableFeatures12.bufferDeviceAddress;
            features12.shaderFloat16 = availableFeatures12.shaderFloat16;

            // Check for ray tracing extension support
            if (supportedExtensions.contains(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) &&
                supportedExtensions.contains(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) &&
                supportedExtensions.contains(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME) &&
                supportedExtensions.contains(VK_KHR_RAY_QUERY_EXTENSION_NAME)) {

                accelerationStructureFeature.accelerationStructure = VK_TRUE;
                rtPipelineFeature.rayTracingPipeline = VK_TRUE;
                rayQueryFeature.rayQuery = VK_TRUE;

                featureBuilder.Append(accelerationStructureFeature);
                featureBuilder.Append(rtPipelineFeature);
                featureBuilder.Append(rayQueryFeature);

                if (supportedExtensions.contains(VK_NV_RAY_TRACING_VALIDATION_EXTENSION_NAME) && instance->validationLayersEnabled) {
                    validationFeatures.rayTracingValidation = VK_TRUE;
                    featureBuilder.Append(validationFeatures);
                }

                support.hardwareRayTracing = true;
            }

            if (supportedExtensions.contains(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME)) {
                support.shaderPrintf = true;
            }

            if (supportedExtensions.contains(VK_EXT_DEBUG_MARKER_EXTENSION_NAME)) {
                support.debugMarker = true;
            }

            if (supportedExtensions.contains(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME)) {
                support.shaderFloat16 = true;
            }

            if (supportedExtensions.contains(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME)) {
                memoryPriorityFeature.memoryPriority = VK_TRUE;

                featureBuilder.Append(memoryPriorityFeature);

                support.memoryPriority = true;
            }

            VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedStateFeatures = {};
            extendedStateFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT;

            if (supportedExtensions.contains(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME)) {
                extendedStateFeatures.extendedDynamicState = VK_TRUE;

                featureBuilder.Append(extendedStateFeatures);

                support.extendedDynamicState = true;
            }

#ifdef AE_BINDLESS
            if (supportedExtensions.contains(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME) &&
                availableFeatures12.descriptorBindingPartiallyBound && availableFeatures12.runtimeDescriptorArray) {

                features12.descriptorBindingPartiallyBound = VK_TRUE;
                features12.runtimeDescriptorArray = VK_TRUE;

                support.bindless = true;
            }
#endif

            support.wideLines = features.features.wideLines;

#ifdef AE_OS_MACOS
            VkPhysicalDevicePortabilitySubsetFeaturesKHR portabilityFeatures = {};

            if (supportedExtensions.contains(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME)) {
                // This is hacked since I can't get it to work otherwise
                // See VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR in vulkan_core.h
                portabilityFeatures.sType = static_cast<VkStructureType>(1000163000);
                portabilityFeatures.mutableComparisonSamplers = VK_TRUE;

                // This feature struct is the last one in the pNext chain for now
                featureBuilder.Append(portabilityFeatures);
            }
#endif
            featureBuilder.Append(features);
            featureBuilder.Append(features11);
            featureBuilder.Append(features12);

            VK_CHECK_MESSAGE(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device), "Error creating graphics device")

        }

        bool GraphicsDevice::CheckForWindowResize() {

            int32_t width, height;
            surface->GetExtent(width, height);

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
                const auto& frameCommandLists = frameData[i].commandLists;
                for (auto commandList : frameCommandLists) {
                    delete commandList;
                }

                vkDestroyFence(device, frameData[i].fence, nullptr);
                vkDestroySemaphore(device, frameData[i].semaphore, nullptr);
            }

        }

        FrameData *GraphicsDevice::GetFrameData(int32_t frameIdx) {

            return &frameData[frameIdx % FRAME_DATA_COUNT];

        }

        void GraphicsDevice::DestroyUnusedGraphicObjects() {

            DeleteOutdatedResources<RenderPass>(renderPasses);
            DeleteOutdatedResources<FrameBuffer>(frameBuffers);
            DeleteOutdatedResources<Shader>(shaders);
            DeleteOutdatedResources<Pipeline>(pipelines);
            DeleteOutdatedResources<Buffer>(buffers);
            DeleteOutdatedResources<MultiBuffer>(multiBuffers);
            DeleteOutdatedResources<Image>(images);
            DeleteOutdatedResources<Sampler>(samplers);
            DeleteOutdatedResources<DescriptorSetLayout>(descriptorSetLayouts);
            DeleteOutdatedResources<DescriptorPool>(descriptorPools);
            DeleteOutdatedResources<QueryPool>(queryPools);
            DeleteOutdatedResources<BLAS>(blases);
            DeleteOutdatedResources<TLAS>(tlases);

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

                std::vector<Ref<Queue>> queues;
                for (auto& queueFamily : queueFamilyIndices.families) {
                    for (auto& queue : queueFamily.queues) {
                        queues.push_back(queue);
                    }
                }

                CommandList *cmd = new CommandList(this, queueType,
                    queueFamilyIndex.value(), queues, frameIndependent);
                cmdLists.push_back(cmd);
                return cmd;
            }

            return *it;

        }

        QueueRef GraphicsDevice::FindAndLockQueue(QueueType queueType) {

            auto threadId = std::this_thread::get_id();

            auto suggestedFamilyIndex = queueFamilyIndices.queueFamilies[queueType].value();
            // Try to find a queue in suggested family index first, should reduce overlapping locks
            auto& suggestedFamily = queueFamilyIndices.families[suggestedFamilyIndex];
            for (auto& queue : suggestedFamily.queues) {
                auto ref = QueueRef(queue, threadId, false);
                if (ref.valid) {
                    return ref;
                }
            }

            // Backup plan, search for all queues
            Ref<Queue> lastSupportedQueue = nullptr;
            for (auto& queueFamily : queueFamilyIndices.families) {
                if ((queueType == GraphicsQueue && !queueFamily.supportsGraphics) ||
                    (queueType == TransferQueue && !queueFamily.supportsTransfer) ||
                    (queueType == PresentationQueue && !queueFamily.supportsPresentation))
                    continue;

                for (auto& queue : queueFamily.queues) {
                    auto ref = QueueRef(queue, threadId, false);
                    if (ref.valid) {
                        return ref;
                    }
                    lastSupportedQueue = queue;
                }
            }

            // No queue could be locked, force hard lock on last supported queue
            return QueueRef(lastSupportedQueue, threadId, true);
        }

        QueueRef GraphicsDevice::FindAndLockQueue(uint32_t familyIndex) {
            auto threadId = std::this_thread::get_id();

            // Try to find a queue in suggested family index first, should reduce overlapping locks
            auto& suggestedFamily = queueFamilyIndices.families[familyIndex];
            for (auto& queue : suggestedFamily.queues) {
                auto ref = QueueRef(queue, threadId, false);
                if (ref.valid) {
                    return ref;
                }
            }

            return QueueRef(suggestedFamily.queues.back(), threadId, true);
        }

    }

}
