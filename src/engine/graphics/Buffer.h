#ifndef AE_GRAPHICSBUFFER_H
#define AE_GRAPHICSBUFFER_H

#include "Common.h"
#include "MemoryManager.h"

namespace Atlas {

    namespace Graphics {

        struct BufferDesc {

        };

        class Buffer {
        public:
            Buffer(MemoryManager* memManager, BufferDesc desc);

            ~Buffer();

            VkBuffer buffer;
            VmaAllocation allocation;

        private:
            MemoryManager* memoryManager;

        };

    }

}

#endif
