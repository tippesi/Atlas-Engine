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
#include "MemoryManager.h"

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
            VkFence fence;

            std::mutex commandListsMutex;
            std::vector<CommandList*> commandLists;

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
            GraphicsDevice(Surface* surface, bool enableValidationLayers = false);

            GraphicsDevice(const GraphicsDevice& that) = delete;

            ~GraphicsDevice();

            GraphicsDevice& operator=(const GraphicsDevice& that) = delete;

            Shader* CreateShader(ShaderDesc shaderDesc);

            Pipeline* CreatePipeline(GraphicsPipelineDesc desc);

            Pipeline* CreatePipeline(ComputePipelineDesc desc);

            Buffer* CreateBuffer(BufferDesc bufferDesc);

            Image* CreateImage(ImageDesc imageDesc);

            Sampler* CreateSampler(SamplerDesc samplerDesc);

            DescriptorPool* CreateDescriptorPool();

            CommandList* GetCommandList(QueueType queueType = QueueType::GraphicsQueue);

            void SubmitCommandList(CommandList* cmd, VkPipelineStageFlags waitStage =
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, ExecutionOrder order = ExecutionOrder::Sequential);

            void CompleteFrame();

            bool CheckFormatSupport(VkFormat format, VkFormatFeatureFlags featureFlags);

            VkQueue GetQueue(QueueType queueType) const;

            void WaitForIdle() const;

            SwapChain* swapChain = nullptr;
            MemoryManager* memoryManager = nullptr;

            VkPhysicalDevice physicalDevice;
            VkDevice device;
            VkPhysicalDeviceProperties deviceProperties;

            bool isComplete = false;

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

            void CreateSwapChain();

            bool CheckForWindowResize();

            void CreateFrameData();

            void DestroyFrameData();

            FrameData* GetFrameData();

            Surface* surface;

            QueueFamilyIndices queueFamilyIndices;

            std::vector<Shader*> shaders;
            std::vector<Pipeline*> pipelines;
            std::vector<Buffer*> buffers;
            std::vector<Image*> images;
            std::vector<Sampler*> samplers;
            std::vector<DescriptorPool*> descriptorPools;

            int32_t frameIndex = 0;
            FrameData frameData[FRAME_DATA_COUNT];

            int32_t windowWidth = 0;
            int32_t windowHeight = 0;
        };

    }

}

#endif
