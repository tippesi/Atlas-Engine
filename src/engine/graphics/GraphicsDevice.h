#ifndef AE_GRAPHICSDEVICE_H
#define AE_GRAPHICSDEVICE_H

#include "Common.h"
#include "Surface.h"
#include "SwapChain.h"
#include "CommandList.h"
#include "Shader.h"
#include "MemoryManager.h"

#include <optional>
#include <vector>

namespace Atlas {

    namespace Graphics {

        class Instance;

        struct FrameData {
            VkFence fence;
        };

        class GraphicsDevice {

        public:
            GraphicsDevice(Surface* surface, bool enableValidationLayers = false);

            GraphicsDevice(const GraphicsDevice& that) = delete;

            ~GraphicsDevice();

            GraphicsDevice& operator=(const GraphicsDevice& that) = delete;

            Shader* CreateShader(ShaderDesc shaderDesc);

            CommandList* GetCommandList(QueueType queueType = QueueType::GraphicsQueue);

            void SubmitCommandList(CommandList* cmd);

            void CompleteFrame();

            SwapChain* swapChain = nullptr;
            MemoryManager* memoryManager = nullptr;

            bool isComplete = false;

        private:
            struct QueueFamilyIndices {
                std::optional<uint32_t> queueFamilies[2];
                VkQueue queues[2];

                bool IsComplete() {
                    return queueFamilies[QueueType::GraphicsQueue].has_value() &&
                        queueFamilies[QueueType::PresentationQueue].has_value();
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

            void CreateSwapChain(Surface* surface);

            void CreateFrameData();

            void DestroyFrameData();

            const FrameData* GetFrameData() const;

            VkPhysicalDevice physicalDevice;
            VkDevice device;

            QueueFamilyIndices queueFamilyIndices;

            std::vector<CommandList*> commandLists;
            std::vector<Shader*> shaders;

            int32_t frameIndex = 0;
            FrameData frameData[FRAME_DATA_COUNT];
        };

    }

}

#endif
