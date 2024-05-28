#pragma once

#include "Common.h"
#include "Surface.h"
#include "Queue.h"
#include "SwapChain.h"
#include "CommandList.h"
#include "Shader.h"
#include "Pipeline.h"
#include "Buffer.h"
#include "Image.h"
#include "Sampler.h"
#include "DescriptorSetLayout.h"
#include "DescriptorPool.h"
#include "QueryPool.h"
#include "BLAS.h"
#include "TLAS.h"
#include "Framebuffer.h"
#include "MemoryManager.h"

#include "../common/Ref.h"

#include <optional>
#include <vector>
#include <mutex>
#include <set>
#include <future>
#include <shared_mutex>

namespace Atlas {

    namespace Graphics {

        class Instance;
        class ImguiWrapper;

        struct DeviceSupport {
            bool hardwareRayTracing = false;
            bool shaderPrintf = false;
            bool bindless = false;
            bool debugMarker = false;
            bool wideLines = false;
            bool shaderFloat16 = false;
            bool extendedDynamicState = false;
        };

        struct CommandListSubmission {
            CommandList* cmd;

            VkPipelineStageFlags waitStage;
        };

        class FrameData {
        public:
            VkSemaphore semaphore = VK_NULL_HANDLE;
            VkFence fence = VK_NULL_HANDLE;
            VkSemaphore submitSemaphore = VK_NULL_HANDLE;

            std::mutex commandListsMutex;
            std::vector<CommandList*> commandLists;

            std::mutex submissionMutex;
            std::vector<CommandList*> submittedCommandLists;

            std::vector<CommandListSubmission> submissions;

            void WaitAndReset(VkDevice device) {
                if (submittedCommandLists.size() > 0) {
                    std::vector<VkFence> fences;
                    for (auto commandList : submittedCommandLists) {
                        fences.push_back(commandList->fence);
                    }
                    VK_CHECK(vkWaitForFences(device, uint32_t(fences.size()), fences.data(), true, 30000000000))
                    VK_CHECK(vkResetFences(device, uint32_t(fences.size()), fences.data()))
                }

                for (auto commandList : submittedCommandLists) {
                    commandList->ResetDescriptors();
                    commandList->frameBufferInUse = nullptr;
                    commandList->renderPassInUse = nullptr;
                    commandList->pipelineInUse = nullptr;
                    commandList->wasSwapChainAccessed = false;
                    commandList->isLocked = false;
                }
                submittedCommandLists.clear();
                submissions.clear();
            }

            void RecreateSemaphore(VkDevice device) {
                vkDestroySemaphore(device, semaphore, nullptr);

                VkSemaphoreCreateInfo semaphoreInfo = Initializers::InitSemaphoreCreateInfo();
                VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore))
            }
        };

        class GraphicsDevice {

            friend ImguiWrapper;

        public:
            explicit GraphicsDevice(Surface* surface, bool enableValidationLayers = false);

            GraphicsDevice(const GraphicsDevice& that) = delete;

            ~GraphicsDevice();

            GraphicsDevice& operator=(const GraphicsDevice& that) = delete;

            SwapChain* CreateSwapChain(VkPresentModeKHR presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR,
                ColorSpace preferredColorSpace = SRGB_NONLINEAR);

            Ref<RenderPass> CreateRenderPass(RenderPassDesc desc);

            Ref<FrameBuffer> CreateFrameBuffer(FrameBufferDesc desc);

            Ref<Shader> CreateShader(ShaderDesc desc);

            Ref<Pipeline> CreatePipeline(GraphicsPipelineDesc desc);

            Ref<Pipeline> CreatePipeline(ComputePipelineDesc desc);

            Ref<Buffer> CreateBuffer(BufferDesc desc);

            Ref<MultiBuffer> CreateMultiBuffer(BufferDesc desc);

            Ref<Image> CreateImage(ImageDesc desc);

            Ref<Sampler> CreateSampler(SamplerDesc desc);

            Ref<DescriptorSetLayout> CreateDescriptorSetLayout(DescriptorSetLayoutDesc desc);

            Ref<DescriptorPool> CreateDescriptorPool(DescriptorPoolDesc desc);

            Ref<QueryPool> CreateQueryPool(QueryPoolDesc desc);

            Ref<BLAS> CreateBLAS(BLASDesc desc);

            Ref<TLAS> CreateTLAS(TLASDesc desc);

            CommandList* GetCommandList(QueueType queueType = QueueType::GraphicsQueue,
                bool frameIndependentList = false);

            void SubmitCommandList(CommandList* cmd, VkPipelineStageFlags waitStage =
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, ExecutionOrder order = ExecutionOrder::Sequential);

            void FlushCommandList(CommandList* cmd);

            bool IsPreviousFrameComplete();

            void WaitForPreviousFrameCompletion();

            void CompleteFrameAsync();

            void CompleteFrame();

            bool CheckFormatSupport(VkFormat format, VkFormatFeatureFlags featureFlags);

            QueueRef GetAndLockQueue(QueueType queueType);

            void WaitForIdle();

            void ForceMemoryCleanup();

            template <class T>
            struct Resources {
                std::vector<Ref<T>> data;
                std::mutex mutex;
            };

            Instance* instance = nullptr;
            SwapChain* swapChain = nullptr;
            MemoryManager* memoryManager = nullptr;
            Surface* surface = nullptr;

            VkPhysicalDevice physicalDevice;
            VkDevice device;

            VkPhysicalDeviceProperties2 deviceProperties = {};
            VkPhysicalDeviceVulkan11Properties deviceProperties11 = {};
            VkPhysicalDeviceVulkan12Properties deviceProperties12 = {};
            VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties = {};
            VkPhysicalDeviceAccelerationStructurePropertiesKHR accelerationStructureProperties = {};
            VkPhysicalDeviceSubgroupSizeControlProperties subgroupSizeControlProperties = {};

            VkPhysicalDeviceFeatures2 features = {};
            VkPhysicalDeviceVulkan11Features features11 = {};
            VkPhysicalDeviceVulkan12Features features12 = {};

            DeviceSupport support;
            std::set<std::string> supportedExtensions;

            Resources<RenderPass> renderPasses;
            Resources<FrameBuffer> frameBuffers;
            Resources<Shader> shaders;
            Resources<Pipeline> pipelines;
            Resources<Buffer> buffers;
            Resources<MultiBuffer> multiBuffers;
            Resources<Image> images;
            Resources<Sampler> samplers;
            Resources<DescriptorSetLayout> descriptorSetLayouts;
            Resources<DescriptorPool> descriptorPools;
            Resources<QueryPool> queryPools;
            Resources<BLAS> blases;
            Resources<TLAS> tlases;

            bool isComplete = false;

            static GraphicsDevice* DefaultDevice;

        private:
            struct QueueFamily {
                uint32_t index;

                std::vector<Ref<Queue>> queues;
                std::vector<float> queuePriorities;

                bool supportsGraphics;
                bool supportsTransfer;
                bool supportsPresentation;
            };

            struct QueueFamilyIndices {
                std::optional<uint32_t> queueFamilies[3];
                std::vector<QueueFamily> families;

                bool IsComplete() {
#ifndef AE_HEADLESS
                    return queueFamilies[QueueType::GraphicsQueue].has_value() &&
                        queueFamilies[QueueType::PresentationQueue].has_value() &&
                        queueFamilies[QueueType::TransferQueue].has_value();
#else
                    return queueFamilies[QueueType::GraphicsQueue].has_value() &&
                        queueFamilies[QueueType::TransferQueue].has_value();
#endif
                }
            }; 

            QueueRef SubmitAllCommandLists();

            void SubmitCommandList(CommandListSubmission* submission, VkSemaphore previousSemaphore,
                VkSemaphore previousFrameSemaphore, const QueueRef& queue, const QueueRef& nextQueue);

            bool SelectPhysicalDevice(VkInstance instance, VkSurfaceKHR surface,
                const std::vector<const char*>& requiredExtensions, std::vector<const char*>& optionalExtensions);

            int32_t RateDeviceSuitability(VkPhysicalDevice device, VkSurfaceKHR surface,
                const std::vector<const char*>& requiredExtensions, std::vector<const char*>& optionalExtensions);

            bool FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

            std::vector<VkDeviceQueueCreateInfo> CreateQueueInfos();

            bool CheckDeviceExtensionSupport(VkPhysicalDevice physicalDevice,
                const std::vector<const char*>& extensionNames);

            std::vector<const char*> CheckDeviceOptionalExtensionSupport(VkPhysicalDevice physicalDevice,
                std::vector<const char*>& extensionNames);

            void BuildPhysicalDeviceFeatures(VkPhysicalDevice device);

            void GetPhysicalDeviceProperties(VkPhysicalDevice device);

            void CreateDevice(const std::vector<VkDeviceQueueCreateInfo>& queueCreateInfos,
                const std::vector<const char*>& extensions, bool enableValidationLayers);

            bool CheckForWindowResize();

            void CreateFrameData();

            void DestroyFrameData();

            FrameData* GetFrameData(int32_t frameIdx);

            void DestroyUnusedGraphicObjects();

            CommandList* GetOrCreateCommandList(QueueType queueType, std::mutex& mutex,
                std::vector<CommandList*>& commandLists, bool frameIndependent);

            QueueRef FindAndLockQueue(QueueType queueType);

            QueueRef FindAndLockQueue(uint32_t familyIndex);

            template<class T>
            void DeleteOutdatedResources(Resources<T>& resources) {
                std::lock_guard<std::mutex> guard(resources.mutex);

                auto& data = resources.data;
                for (size_t i = 0; i < data.size(); i++) {
                    auto& ref = data[i];
                    if (ref.use_count() == 1) {
                        ref.swap(data.back());
                        memoryManager->DestroyAllocation(data.back());
                        data.pop_back();
                        i--;
                    }
                }
            }

            QueueFamilyIndices queueFamilyIndices;

            std::mutex commandListsMutex;
            std::vector<CommandList*> commandLists;

            int32_t frameIndex = 0;
            FrameData frameData[FRAME_DATA_COUNT];

            int32_t windowWidth = 0;
            int32_t windowHeight = 0;

            std::shared_mutex queueMutex;

            std::atomic_bool frameComplete = true;
            std::future<void> completeFrameFuture;

        };

    }

}