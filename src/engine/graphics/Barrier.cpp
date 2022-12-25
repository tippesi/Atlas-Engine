#include "Barrier.h"

namespace Atlas {

    namespace Graphics {

        BufferBarrier::BufferBarrier(VkAccessFlags newAccessMask) : newAccessMask(newAccessMask) {



        }

        BufferBarrier& BufferBarrier::Update(const Ref<Atlas::Graphics::Buffer> &buffer) {

            barrier = Initializers::InitBufferMemoryBarrier(buffer->buffer,
                buffer->accessMask, newAccessMask);

            return *this;

        }

        ImageBarrier::ImageBarrier(VkImageLayout newLayout, VkAccessFlags newAccessMask)
            : newLayout(newLayout), newAccessMask(newAccessMask) {



        }

        ImageBarrier& ImageBarrier::Update(const Ref<Atlas::Graphics::Image> &image) {

            this->image = image.get();
            barrier = Initializers::InitImageMemoryBarrier(image->image, image->layout,
                newLayout, image->accessMask, newAccessMask, image->aspectFlags);

            return *this;

        }

    }

}