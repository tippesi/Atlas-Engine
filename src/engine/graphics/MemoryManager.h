#ifndef AE_GRAPHICSMEMORYMANAGER_H
#define AE_GRAPHICSMEMORYMANAGER_H

#include "Common.h"

#define VMA_STATS_STRING_ENABLED 0
#include <vk_mem_alloc.h>

namespace Atlas {

    namespace Graphics {

        class MemoryManager {

        public:
            MemoryManager(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);

            ~MemoryManager();

            VmaAllocator allocator;

            VkInstance instance;
            VkPhysicalDevice physicalDevice;
            VkDevice device;

        private:


        };

    }

}

#endif
