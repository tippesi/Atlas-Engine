#ifndef AE_GRAPHICSBUFFER_H
#define AE_GRAPHICSBUFFER_H

#include "Common.h"
#include "MemoryManager.h"

namespace Atlas {

    namespace Graphics {

        struct BufferDesc {
            VkBufferUsageFlags usageFlags;

            void* data = nullptr;
            size_t size;
        };

        class Buffer {
        public:
            Buffer(MemoryManager* memManager, BufferDesc desc);

            ~Buffer();

            void SetData(void* data, size_t offset, size_t length);

            VkBuffer buffer;
            VmaAllocation allocation;

        private:
            MemoryManager* memoryManager;

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
