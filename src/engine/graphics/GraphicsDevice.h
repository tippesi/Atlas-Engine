#ifndef AE_GRAPHICSDEVICE_H
#define AE_GRAPHICSDEVICE_H

#include "Common.h"
#include "Surface.h"
#include "SwapChain.h"
#include "CommandList.h"
#include "Shader.h"
#include "Pipeline.h"
#include "Buffer.h"
#include "Image.h"
#include "Sampler.h"
#include "Descriptor.h"
#include "QueryPool.h"
#include "Framebuffer.h"
#include "MemoryManager.h"

#include "../common/Ref.h"

#include <optional>
#include <vector>
#include <mutex>

namespace Atlas {

    namespace Graphics {

        class Instance;
        class ImguiWrapper;

        enum class ExecutionOrder {
            Sequential = 0,
            Parallel = 1
        };

        class FrameData {
        public:
            VkSemaphore semaphore;
            VkFence fence;

            std::mutex commandListsMutex;
            std::vector<CommandList*> commandLists;

            std::mutex submissionMutex;
            std::vector<CommandList*> submittedCommandLists;

            void WaitAndReset(VkDevice device) {
                if (submittedCommandLists.size() > 0) {
                    std::vector<VkFence> fences;
                    for (auto commandList : submittedCommandLists) {
                        fences.push_back(commandList->fence);
                    }
                    VK_CHECK(vkWaitForFences(device, uint32_t(fences.size()), fences.data(), true, 1000000000))
                    VK_CHECK(vkResetFences(device, uint32_t(fences.size()), fences.data()))
                }

                for (auto commandList : submittedCommandLists) {
                    commandList->ResetDescriptors();
                    commandList->isLocked = false;
                }
                submittedCommandLists.clear();
            }
        };

        class GraphicsDevice {

            friend ImguiWrapper;

        public:
            explicit GraphicsDevice(Surface* surface, bool enableValidationLayers = false);

            GraphicsDevice(const GraphicsDevice& that) = delete;

            ~GraphicsDevice();

            GraphicsDevice& operator=(const GraphicsDevice& that) = delete;

            SwapChain* CreateSwapChain(VkPresentModeKHR presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR);

            Ref<RenderPass> CreateRenderPass(RenderPassDesc desc);

            Ref<FrameBuffer> CreateFrameBuffer(FrameBufferDesc desc);

            Ref<Shader> CreateShader(ShaderDesc desc);

            Ref<Pipeline> CreatePipeline(GraphicsPipelineDesc desc);

            Ref<Pipeline> CreatePipeline(ComputePipelineDesc desc);

            Ref<Buffer> CreateBuffer(BufferDesc desc);

            Ref<MultiBuffer> CreateMultiBuffer(BufferDesc desc);

            Ref<Image> CreateImage(ImageDesc desc);

            Ref<Sampler> CreateSampler(SamplerDesc desc);

            Ref<DescriptorPool> CreateDescriptorPool();

            Ref<QueryPool> CreateQueryPool(QueryPoolDesc desc);

            CommandList* GetCommandList(QueueType queueType = QueueType::GraphicsQueue,
                bool frameIndependentList = false);

            void SubmitCommandList(CommandList* cmd, VkPipelineStageFlags waitStage =
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, ExecutionOrder order = ExecutionOrder::Sequential);

            void FlushCommandList(CommandList* cmd);

            void CompleteFrame();

            bool CheckFormatSupport(VkFormat format, VkFormatFeatureFlags featureFlags);

            VkQueue GetQueue(QueueType queueType) const;

            void WaitForIdle() const;

            void ForceMemoryCleanup();

            Instance* instance = nullptr;
            SwapChain* swapChain = nullptr;
            MemoryManager* memoryManager = nullptr;
            Surface* surface = nullptr;

            VkPhysicalDevice physicalDevice;
            VkDevice device;
            VkPhysicalDeviceProperties deviceProperties;

            bool isComplete = false;

            static GraphicsDevice* DefaultDevice;

        private:
            struct QueueFamilyIndices {
                std::optional<uint32_t> queueFamilies[3];
                VkQueue queues[3];

                bool IsComplete() {
                    return queueFamilies[QueueType::GraphicsQueue].has_value() &&
                        queueFamilies[QueueType::PresentationQueue].has_value() &&
                        queueFamilies[QueueType::TransferQueue].has_value();
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

            bool CheckForWindowResize();

            void CreateFrameData();

            void DestroyFrameData();

            FrameData* GetFrameData();

            void DestroyUnusedGraphicObjects();

            CommandList* GetOrCreateCommandList(QueueType queueType, std::mutex& mutex,
                std::vector<CommandList*>& commandLists, bool frameIndependent);

            QueueFamilyIndices queueFamilyIndices;

            std::vector<Ref<RenderPass>> renderPasses;
            std::vector<Ref<FrameBuffer>> frameBuffers;
            std::vector<Ref<Shader>> shaders;
            std::vector<Ref<Pipeline>> pipelines;
            std::vector<Ref<Buffer>> buffers;
            std::vector<Ref<MultiBuffer>> multiBuffers;
            std::vector<Ref<Image>> images;
            std::vector<Ref<Sampler>> samplers;
            std::vector<Ref<DescriptorPool>> descriptorPools;
            std::vector<Ref<QueryPool>> queryPools;

            std::mutex commandListsMutex;
            std::vector<CommandList*> commandLists;

            int32_t frameIndex = 0;
            FrameData frameData[FRAME_DATA_COUNT];

            int32_t windowWidth = 0;
            int32_t windowHeight = 0;
        };

    }

}

#endif
