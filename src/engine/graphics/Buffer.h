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

    }

}

#endif
