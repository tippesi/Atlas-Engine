#include "Texture2D.h"

namespace Atlas {

    namespace Texture {

        Texture2D::Texture2D(int32_t width, int32_t height, VkFormat format, Wrapping wrapping, Filtering filtering) {

            this->format = format;

            Reallocate(Graphics::ImageType::Image2D, width, height, 1, filtering, wrapping);
            RecreateSampler(filtering, wrapping);

        }

        Texture2D::Texture2D(const std::string& filename, bool colorSpaceConversion,
            Wrapping wrapping, Filtering filtering, int32_t forceChannels) {

            auto image = Loader::ImageLoader::LoadImage<uint8_t>(filename,
                colorSpaceConversion, forceChannels);
            InitializeInternal(image, wrapping, filtering);

        }

        Texture2D::Texture2D(const Ref<Common::Image<uint8_t>>& image, Wrapping wrapping, Filtering filtering) {

            InitializeInternal(image, wrapping, filtering);

        }

        Texture2D::Texture2D(const Ref<Common::Image<float>>& image, Wrapping wrapping, Filtering filtering) {

            InitializeInternal(image, wrapping, filtering);

        }

        void Texture2D::Resize(int32_t width, int32_t height) {

            if (width != this->width || height != this->height) {

                Reallocate(Graphics::ImageType::Image2D, width, height, 1, filtering, wrapping);

            }

        }

    }

}