#pragma once

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

            BufferBarrier(const Ref<MultiBuffer>& buffer, VkAccessFlags newAccessMask);

            BufferBarrier& Update(const Ref<Buffer>& buffer);

            BufferBarrier& Update(const Ref<MultiBuffer>& buffer);

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