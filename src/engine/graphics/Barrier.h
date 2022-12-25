#ifndef AE_GRAPHICSBARRIER_H
#define AE_GRAPHICSBARRIER_H

#include "Common.h"

#include "Buffer.h"
#include "Image.h"

namespace Atlas {

    namespace Graphics {

        class BufferBarrier {
        public:
            BufferBarrier() = default;

            BufferBarrier(VkAccessFlags newAccessMask);

            BufferBarrier& Update(const Ref<Buffer>& buffer);

            Buffer* buffer = nullptr;

            VkAccessFlags newAccessMask;

            VkBufferMemoryBarrier barrier = {};

        };

        class ImageBarrier {
        public:
            ImageBarrier() = default;

            ImageBarrier(VkImageLayout newLayout, VkAccessFlags newAccessMask);

            ImageBarrier& Update(const Ref<Image>& image);

            Image* image = nullptr;

            VkImageLayout newLayout;
            VkAccessFlags newAccessMask;

            VkImageMemoryBarrier barrier = {};

        };

    }

}

#endif