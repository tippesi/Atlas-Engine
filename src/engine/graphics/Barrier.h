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

            BufferBarrier(VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask);

            BufferBarrier& Update(const Ref<Buffer>& buffer);

            Buffer* image = nullptr;

            VkAccessFlags srcAccessMask;
            VkAccessFlags dstAccessMask;

            VkBufferMemoryBarrier barrier = {};

        };

        class ImageBarrier {
        public:
            ImageBarrier() = default;

            ImageBarrier(VkImageLayout newLayout, VkAccessFlags srcAccessMask,
                VkAccessFlags dstAccessMask);

            ImageBarrier& Update(const Ref<Image>& image);

            Image* image = nullptr;

            VkImageLayout newLayout;
            VkAccessFlags srcAccessMask;
            VkAccessFlags dstAccessMask;

            VkImageMemoryBarrier barrier = {};

        };

    }

}

#endif