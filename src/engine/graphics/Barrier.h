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

            explicit BufferBarrier(VkAccessFlags newAccessMask);

            BufferBarrier(const Ref<Buffer>& buffer, VkAccessFlags newAccessMask);

            BufferBarrier& Update(const Ref<Buffer>& buffer);

            Buffer* buffer = nullptr;

            VkAccessFlags newAccessMask;

            VkBufferMemoryBarrier barrier = {};

        };

        class ImageBarrier {
        public:
            ImageBarrier() = default;

            ImageBarrier(VkImageLayout newLayout, VkAccessFlags newAccessMask);

            ImageBarrier(const Ref<Image>& image, VkImageLayout newLayout, VkAccessFlags newAccessMask);

            ImageBarrier& Update(const Ref<Image>& image);

            Image* image = nullptr;

            VkImageLayout newLayout;
            VkAccessFlags newAccessMask;

            VkImageMemoryBarrier barrier = {};

        };

    }

}

#endif