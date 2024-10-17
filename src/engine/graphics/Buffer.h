#pragma once

#include "Common.h"

#include "../common/Ref.h"

#define VMA_STATS_STRING_ENABLED 0
#include <vk_mem_alloc.h>

namespace Atlas {

    namespace Graphics {

        enum class BufferDomain {
            Device = 0,
            Host = 1
        };

        enum BufferHostAccess {
            Sequential = 0,
            Random = 1
        };

        struct BufferDesc {
            VkBufferUsageFlags usageFlags;

            BufferDomain domain = BufferDomain::Device;
            // This one is only used when accessing data from the host directly
            BufferHostAccess hostAccess = BufferHostAccess::Sequential;

            void* data = nullptr;
            size_t dataSize = 0;

            size_t size;
            size_t alignment = 0;

            bool dedicatedMemory = false;
            float priority = 0.5f;
        };

        struct BufferAllocation {
            VkBuffer buffer;
            VmaAllocation allocation;
        };

        class GraphicsDevice;
        class MemoryManager;

        class Buffer {
        public:
            Buffer(GraphicsDevice* device, const BufferDesc& desc);

            ~Buffer();

            void SetData(void* data, size_t offset, size_t length);

            void* Map();

            void Unmap();

            VkDeviceAddress GetDeviceAddress();

            VkBuffer buffer;
            VmaAllocation allocation;
            VkAccessFlags accessMask = VK_ACCESS_MEMORY_READ_BIT |
                VK_ACCESS_MEMORY_WRITE_BIT;
            VkBufferUsageFlags usageFlags;

            BufferDomain domain;

            const size_t size = 0;
            const size_t alignment = 0;

        private:
            MemoryManager* memoryManager;

            bool isMapped = false;
            void* mappedData = nullptr;

        public:
            static size_t GetAlignedSize(size_t size);

        };

        class MultiBuffer {
            friend GraphicsDevice;
        public:
            MultiBuffer(GraphicsDevice* device, BufferDesc& desc);

            ~MultiBuffer();

            void SetData(void* data, size_t offset, size_t length);

            void* Map();

            void Unmap();

            Buffer* GetCurrent() const;

            const size_t size = 0;

        private:
            void UpdateFrameIndex(size_t frameIndex);

            size_t frameIndex = 0;
            Buffer* frameBuffer[FRAME_DATA_COUNT];
        };

        // Lightweight specializations of the buffer class
        struct IndexBuffer {
            Ref<Buffer> buffer = nullptr;
            VkIndexType type = {};
        };

        struct VertexBuffer {
            Ref<Buffer> buffer = nullptr;
            VkVertexInputBindingDescription bindingDescription = {};
            VkVertexInputAttributeDescription attributeDescription = {};
        };

    }

}