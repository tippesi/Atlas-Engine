#include "Barrier.h"

namespace Atlas {

    namespace Graphics {

        BufferBarrier::BufferBarrier(VkAccessFlags newAccessMask) : newAccessMask(newAccessMask) {



        }

        BufferBarrier::BufferBarrier(const Ref<Buffer> &buffer, VkAccessFlags newAccessMask)
            : newAccessMask(newAccessMask) {

            Update(buffer);

        }

        BufferBarrier::BufferBarrier(const Ref<MultiBuffer> &buffer, VkAccessFlags newAccessMask)
            : newAccessMask(newAccessMask) {

            Update(buffer);

        }

        BufferBarrier& BufferBarrier::Update(const Ref<Buffer> &buffer) {

            this->buffer = buffer.get();
            barrier = Initializers::InitBufferMemoryBarrier(buffer->buffer,
                buffer->accessMask, newAccessMask);

            buffer->accessMask = newAccessMask;

            return *this;

        }

        BufferBarrier& BufferBarrier::Update(const Ref<MultiBuffer> &buffer) {

            this->buffer = buffer->GetCurrent();
            barrier = Initializers::InitBufferMemoryBarrier(this->buffer->buffer,
                this->buffer->accessMask, newAccessMask);

            buffer->GetCurrent()->accessMask = newAccessMask;

            return *this;

        }

        ImageBarrier::ImageBarrier(VkImageLayout newLayout, VkAccessFlags newAccessMask)
            : newLayout(newLayout), newAccessMask(newAccessMask) {



        }

        ImageBarrier::ImageBarrier(const Ref<Image> &image, VkImageLayout newLayout, VkAccessFlags newAccessMask)
            : newLayout(newLayout), newAccessMask(newAccessMask) {

            Update(image);

        }

        ImageBarrier::ImageBarrier(Image* image, VkImageLayout newLayout, VkAccessFlags newAccessMask)
            : newLayout(newLayout), newAccessMask(newAccessMask) {

            Update(image);

        }

        ImageBarrier& ImageBarrier::Update(const Ref<Image> &image) {

            return Update(image.get());

        }

        ImageBarrier& ImageBarrier::Update(Image* image) {

            this->image = image;
            barrier = Initializers::InitImageMemoryBarrier(image->image, image->layout,
                newLayout, image->accessMask, newAccessMask, image->aspectFlags);
            barrier.subresourceRange.layerCount = image->layers;
            barrier.subresourceRange.levelCount = image->mipLevels;

            image->accessMask = newAccessMask;
            image->layout = newLayout;

            return *this;

        }

    }

}