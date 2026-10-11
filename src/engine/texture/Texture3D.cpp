#include "Texture3D.h"

namespace Atlas {

    namespace Texture {

        Texture3D::Texture3D(int32_t width, int32_t height, int32_t depth, VkFormat format,
            Wrapping wrapping, Filtering filtering, bool dedicatedMemory, bool usedForRenderTarget) {

            this->format = format;
            Reallocate(Graphics::ImageType::Image3D, width, height, depth, filtering, wrapping, dedicatedMemory, usedForRenderTarget);
            RecreateSampler(filtering, wrapping);

        }

        void Texture3D::Resize(int32_t width, int32_t height, int32_t depth) {

            if (width != this->width || height != this->height ||
                depth != this->depth) {

                Reallocate(Graphics::ImageType::Image3D, width, height, depth, 
                    filtering, wrapping, dedicatedMemory, usedForRenderTarget);

            }

        }

    }

}