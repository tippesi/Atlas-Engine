#include "Texture2D.h"
#include "../graphics/Format.h"

namespace Atlas {

    namespace Texture {

        Texture2D::Texture2D(int32_t width, int32_t height, VkFormat format, Wrapping wrapping, Filtering filtering,
            bool dedicatedMemory, bool usedForRenderTarget) {

            this->format = format;

            Reallocate(Graphics::ImageType::Image2D, width, height, 1, filtering, wrapping, dedicatedMemory, usedForRenderTarget);
            RecreateSampler(filtering, wrapping);

        }

        Texture2D::Texture2D(const std::string& filename, bool colorSpaceConversion,
            Wrapping wrapping, Filtering filtering, int32_t forceChannels,
            bool dedicatedMemory, bool usedForRenderTarget) {

            if (Loader::KtxLoader::IsKtxFile(filename)) {
                auto image = Loader::KtxLoader::LoadImage(filename, colorSpaceConversion);
                InitializeInternal(image, wrapping, filtering, dedicatedMemory);
                return;
            }

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

        Texture2D::Texture2D(const Ref<Loader::KtxImage>& image, Wrapping wrapping, Filtering filtering,
            bool dedicatedMemory) {

            InitializeInternal(image, wrapping, filtering, dedicatedMemory);

        }

        void Texture2D::Resize(int32_t width, int32_t height) {

            if (width != this->width || height != this->height) {

                Reallocate(Graphics::ImageType::Image2D, width, height, 1, 
                    filtering, wrapping, dedicatedMemory, usedForRenderTarget);

            }

        }

        void Texture2D::InitializeInternal(const Ref<Loader::KtxImage>& image, Wrapping wrapping,
            Filtering filtering, bool dedicatedMemory) {

            format = image->format;

            bool generateMipMaps = filtering == Filtering::MipMapLinear ||
                filtering == Filtering::MipMapNearest || filtering == Filtering::Anisotropic;

            if (generateMipMaps && image->mipLevels == 1 && !Graphics::IsFormatCompressed(format)) {
                Reallocate(Graphics::ImageType::Image2D, image->width, image->height, 1, filtering, wrapping, dedicatedMemory);
                RecreateSampler(filtering, wrapping);

                this->image->SetData(image->data.data(), 0, 0, 0, uint32_t(width), uint32_t(height), 1);
                return;
            }

            Reallocate(Graphics::ImageType::Image2D, image->width, image->height, 1, filtering, wrapping,
                dedicatedMemory, false, image->mipLevels);
            RecreateSampler(filtering, wrapping);

            std::vector<VkBufferImageCopy> copyRegions(image->copyRegions.begin(),
                image->copyRegions.begin() + this->image->mipLevels);
            this->image->SetData(image->data.data(), image->data.size(), copyRegions);

        }

    }

}