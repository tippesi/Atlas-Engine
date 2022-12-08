#ifndef AE_GRAPHICSMEMORYALLOCATOR_H
#define AE_GRAPHICSMEMORYALLOCATOR_H

#include <volk.h>

#define VMA_IMPLEMENTATION
#define VMA_STATS_STRING_ENABLED 0
#include <vk_mem_alloc.h>

namespace Atlas {

    namespace Graphics {

        struct MemoryAllocatorDesc {
            VkInstance instance;
            VkPhysicalDevice physicalDevice;
            VkDevice device;
        };

        class MemoryAllocator {

        public:
            MemoryAllocator(const MemoryAllocatorDesc desc, bool& success);

            VmaAllocator allocator;

        };

    }

}

#endif
