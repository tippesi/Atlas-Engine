#include "Texture2D.h"

namespace Atlas {

    namespace Texture {

        Texture2D::Texture2D(int32_t width, int32_t height, VkFormat format, Wrapping wrapping, Filtering filtering) {

            this->format = format;

            Reallocate(width, height, 1, filtering, wrapping);
            RecreateSampler(filtering, wrapping);

        }

        Texture2D::Texture2D(std::string filename, bool colorSpaceConversion,
            Filtering filtering, int32_t forceChannels) {

			auto image = Loader::ImageLoader::LoadImage<uint8_t>(filename,
					colorSpaceConversion, forceChannels);
			InitializeInternal(image, Wrapping::Repeat, filtering);

        }

		Texture2D::Texture2D(Common::Image<uint8_t>& image, Filtering filtering) {

            InitializeInternal(image, Wrapping::Repeat, filtering);

		}

        void Texture2D::Resize(int32_t width, int32_t height) {

			if (width != this->width || height != this->height) {

                Reallocate(width, height, depth, filtering, wrapping);

			}

        }

        void Texture2D::InitializeInternal(Atlas::Common::Image<uint8_t> &image, Wrapping wrapping,
            Filtering filtering) {

            switch(image.channels) {
                case 1: format = VK_FORMAT_R8_UNORM; break;
                case 2: format = VK_FORMAT_R8G8_UNORM; break;
                case 3: format = VK_FORMAT_R8G8B8_UNORM; break;
                default: format = VK_FORMAT_R8G8B8A8_UNORM; break;
            }

            Reallocate(image.width, image.height, 1, filtering, wrapping);
            RecreateSampler(filtering, wrapping);

			SetData(image.GetData());

		}

    }

}