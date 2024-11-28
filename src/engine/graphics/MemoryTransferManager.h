#pragma once

#include "Common.h"
#include "Queue.h"

#define VMA_STATS_STRING_ENABLED 0
#include <vk_mem_alloc.h>
#include <functional>
#include <vector>

namespace Atlas {

    namespace Graphics {

        class GraphicsDevice;
        class MemoryManager;
        class Buffer;
        class Image;
        class CommandList;

        class MemoryTransferManager {

        public:
            MemoryTransferManager(GraphicsDevice* device, MemoryManager* memoryManager);

            ~MemoryTransferManager();

            void BeginMultiTransfer();

            void EndMultiTransfer();

            void UploadBufferData(void* data, Buffer* buffer, VkBufferCopy bufferCopyDesc);

            void UploadImageData(void* data, Image* image, VkOffset3D offset, VkExtent3D extent,
                uint32_t layerOffset, uint32_t layerCount);

            // Doesn't work with the multi-transfer yet
            void RetrieveImageData(void* data, Image* image, VkOffset3D offset, VkExtent3D extent,
                uint32_t layerOffset, uint32_t layerCount, bool block = true);

            void GenerateMipMaps(Image* image);

            void GenerateMipMaps(Image* image, VkCommandBuffer cmd);

            void ImmediateSubmit(QueueType queueType, std::function<void(CommandList*)>&& function);

        private:
            struct StagingBufferAllocation {
                VkBuffer buffer;
                VmaAllocation allocation;
            };

            StagingBufferAllocation CreateStagingBuffer(size_t size);

            void DestroyStagingBuffer(StagingBufferAllocation& allocation);

            GraphicsDevice* device = nullptr;
            MemoryManager* memoryManager = nullptr;
            CommandList* commandList = nullptr;

            std::vector<StagingBufferAllocation> allocations;

        };

    }

}