#ifndef AE_GRAPHICSMEMORYMANAGER_H
#define AE_GRAPHICSMEMORYMANAGER_H

#include "Common.h"
#include "MemoryTransferManager.h"

#define VMA_STATS_STRING_ENABLED 0
#include <vk_mem_alloc.h>

#include <deque>

namespace Atlas {

    namespace Graphics {

        struct BufferAllocation {
            VkBuffer buffer;
            VmaAllocation allocation;
        };

        struct ImageAllocation {
            VkImage image;
            VmaAllocation allocation;
        };

        // Forward declare such that it can be a friend
        class GraphicsDevice;

        class MemoryManager {

            friend GraphicsDevice;

        public:
            MemoryManager(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device,
                uint32_t transferQueueFamilyIndex, VkQueue transferQueue);

            MemoryManager(const MemoryManager& that) = delete;

            ~MemoryManager();

            MemoryManager& operator=(const MemoryManager& that) = delete;

            void DestroyAllocation(BufferAllocation allocation);

            void DestroyAllocation(ImageAllocation allocation);

            VmaAllocator allocator;

            VkInstance instance;
            VkPhysicalDevice physicalDevice;
            VkDevice device;

            VkPhysicalDeviceProperties deviceProperties;

            MemoryTransferManager* transferManager;

        private:
            struct DeleteBufferAllocation {
                BufferAllocation allocation;
                size_t deleteFrame;
            };

            struct DeleteImageAllocation {
                ImageAllocation allocation;
                size_t deleteFrame;
            };

            void UpdateFrameIndex(size_t frameIndex);

            void DeleteData();

            const size_t framesToDeletion = 3;

            size_t frameIndex = 0;
            std::deque<DeleteBufferAllocation> deleteBufferAllocations;
            std::deque<DeleteImageAllocation> deleteImageAllocations;

        };

    }

}

#endif
