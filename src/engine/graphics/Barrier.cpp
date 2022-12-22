#include "Barrier.h"

namespace Atlas {

    namespace Graphics {

        BufferBarrier::BufferBarrier(VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask) :
            srcAccessMask(srcAccessMask), dstAccessMask(dstAccessMask) {



        }

        void BufferBarrier::Update(const Ref<Atlas::Graphics::Buffer> &buffer) {

            barrier = Initializers::InitBufferMemoryBarrier(buffer->buffer,
                srcAccessMask, dstAccessMask);

        }

        ImageBarrier::ImageBarrier(VkImageLayout newLayout, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask)
            : newLayout(newLayout), srcAccessMask(srcAccessMask), dstAccessMask(dstAccessMask) {



        }

        void ImageBarrier::Update(const Ref<Atlas::Graphics::Image> &image) {

            this->image = image.get();
            barrier = Initializers::InitImageMemoryBarrier(image->image, image->layout,
                newLayout, srcAccessMask, dstAccessMask, image->aspectFlags);

        }

    }

}