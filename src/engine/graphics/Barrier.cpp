#include "Barrier.h"

namespace Atlas {

    namespace Graphics {

        BufferBarrier::BufferBarrier(VkAccessFlags newAccessMask) : newAccessMask(newAccessMask) {



        }

        BufferBarrier::BufferBarrier(const Ref<Buffer> &buffer, VkAccessFlags newAccessMask)
            : newAccessMask(newAccessMask) {

            Update(buffer);

        }

        BufferBarrier& BufferBarrier::Update(const Ref<Buffer> &buffer) {

            this->buffer = buffer.get();
            barrier = Initializers::InitBufferMemoryBarrier(buffer->buffer,
                buffer->accessMask, newAccessMask);

            return *this;

        }

        ImageBarrier::ImageBarrier(VkImageLayout newLayout, VkAccessFlags newAccessMask)
            : newLayout(newLayout), newAccessMask(newAccessMask) {



        }

        ImageBarrier::ImageBarrier(const Ref<Image> &image, VkImageLayout newLayout, VkAccessFlags newAccessMask)
            : newLayout(newLayout), newAccessMask(newAccessMask) {

            Update(image);

        }

        ImageBarrier& ImageBarrier::Update(const Ref<Image> &image) {

            this->image = image.get();
            barrier = Initializers::InitImageMemoryBarrier(image->image, image->layout,
                newLayout, image->accessMask, newAccessMask, image->aspectFlags);
            barrier.subresourceRange.layerCount = image->layers;
            barrier.subresourceRange.levelCount = image->mipLevels;

            return *this;

        }

    }

}