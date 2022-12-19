#ifndef AE_GRAPHICSBUFFER_H
#define AE_GRAPHICSBUFFER_H

#include "Common.h"
#include "MemoryManager.h"

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
            BufferHostAccess hostAccess = BufferHostAccess::Random;

            void* data = nullptr;
            size_t size;
        };

        class GraphicsDevice;

        class Buffer {
        public:
            Buffer(GraphicsDevice* device, BufferDesc& desc);

            ~Buffer();

            void SetData(void* data, size_t offset, size_t length);

            void Map();

            void Unmap();

            VkBuffer buffer;
            VmaAllocation allocation;

            BufferDomain domain;

        private:
            MemoryManager* memoryManager;

            bool isMapped = false;
            void* mappedData = nullptr;

        };

        class MultiBuffer {
            friend GraphicsDevice;
        public:
            MultiBuffer(GraphicsDevice* device, BufferDesc& desc);

            ~MultiBuffer();

            void SetData(void* data, size_t offset, size_t length);

            Buffer* GetCurrent() const;

        private:
            void UpdateFrameIndex(size_t frameIndex);

            size_t frameIndex = 0;
            Buffer* frameBuffer[FRAME_DATA_COUNT];
        };

        // Lightweight specializations of the buffer class
        struct IndexBuffer {
            Graphics::Buffer* buffer = nullptr;
            VkIndexType type = {};
        };

        struct VertexBuffer {
            Graphics::Buffer* buffer = nullptr;
            VkVertexInputBindingDescription bindingDescription = {};
            VkVertexInputAttributeDescription attributeDescription = {};
        };

    }

}

#endif
