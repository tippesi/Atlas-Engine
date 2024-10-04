#pragma once

#include "../System.h"
#include "Texture.h"

#include "../common/Image.h"
#include "../loader/ImageLoader.h"

namespace Atlas {

    namespace Texture {

        class Texture2D : public Texture {

        public:
            /**
             * Constructs a Texture2D object.
             */
            Texture2D() = default;

            /**
             * Construct a Texture2D object.
             * @param width The width of the texture.
             * @param height The height of the texture.
             * @param format The texture format.
             * @param wrapping The wrapping of the texture. Controls texture border behaviour.
             * @param filtering The filtering of the texture.
             */
            Texture2D(int32_t width, int32_t height, VkFormat format, Wrapping wrapping = Wrapping::Repeat,
                Filtering filtering = Filtering::Nearest, bool dedicatedMemory = false, bool usedForRenderTarget = false);

            /**
            * Constructs a Texture2D object from an image file.
            * @param filename The filename of the image
            * @param colorSpaceConversion Whether or not a sRGB to RGB conversion should be done
            * @param wrapping The wrapping of the texture. Controls texture border behaviour.
            * @param filtering The filtering of the texture.
            * @param forceChannels Can be used to force a number of channels which should be loaded from the file.
            */
            explicit Texture2D(const std::string& filename, bool colorSpaceConversion = true,
                Wrapping wrapping = Wrapping::Repeat, Filtering filtering = Filtering::Anisotropic,
                int32_t forceChannels = 0, bool dedicatedMemory = false, bool usedForRenderTarget = false);

            /**
             * Constructs a Texture2D object from an image object.
             * @param image The image object.
             * @param wrapping The wrapping of the texture. Controls texture border behaviour.
             * @param filtering The filtering of the texture.
             */
            explicit Texture2D(const Ref<Common::Image<uint8_t>>& image, Wrapping wrapping = Wrapping::Repeat,
                Filtering filtering = Filtering::Anisotropic);

            /**
             * Constructs a Texture2D object from an image object.
             * @param image The image object.
             * @param wrapping The wrapping of the texture. Controls texture border behaviour.
             * @param filtering The filtering of the texture.
             */
            explicit Texture2D(const Ref<Common::Image<float>>& image, Wrapping wrapping = Wrapping::Repeat,
                Filtering filtering = Filtering::Anisotropic);

            /**
             * Resizes the texture
             * @param width The new width of the texture.
             * @param height The new height of the texture.
             * @warning This results in a loss of texture data and a
             * change of the texture id.
             */
            void Resize(int32_t width, int32_t height);

            /**
             * Saves the texture to a the supported format.
             * @param filename The name of the file (without a file type, which is automatically selectecd)
             * @note Only uint8_t, uint16_t and float are supported typenames.
             */
            template<typename T>
            void Save(std::string& filename, bool flipHorizontally = false);

        private:
            template<typename T>
            void InitializeInternal(const Ref<Common::Image<T>>& image, Wrapping wrapping,
                Filtering filtering, bool dedicatedMemory = false, bool usedForRenderTarget = false);

        };

        template<typename T>
        void Texture2D::Save(std::string& filename, bool flipHorizontally) {

            auto image = CreateRef<Common::Image<T>>(width, height, channels);

            std::vector<T> data;

            if constexpr (std::is_same_v<T, uint8_t>) {
                image->fileFormat = Common::ImageFormat::PNG;
                filename += ".png";
                data = GetData<T>();
            }
            else if constexpr (std::is_same_v<T, uint16_t>) {
                image->fileFormat = Common::ImageFormat::PGM;
                filename += ".pgm";
                data = GetData<T>();
            }
            else if constexpr (std::is_same_v<T, float>) {
                image->fileFormat = Common::ImageFormat::HDR;
                filename += ".hdr";
                // We have a lot of 16bit float textures, need some care
                if (this->image->bitDepth < 32) {
                    auto texData = GetData<float16>();
                    std::transform(texData.begin(), texData.end(), std::back_inserter(data),
                        [](float16 x) {
                            return glm::detail::toFloat32(x);
                        });
                }
                else {
                    data = GetData<T>();
                }
            }

            image->SetData(data);

            if (flipHorizontally)
                image->FlipHorizontally();

            Loader::ImageLoader::SaveImage(image, filename);

        }

        template<typename T>
        void Texture2D::InitializeInternal(const Ref<Common::Image<T>>& image, Wrapping wrapping, Filtering filtering,
            bool dedicatedMemory, bool usedForRenderTarget) {

            // RGB images are mostly not supported
            if (image->channels == 3) {
                image->ExpandToChannelCount(4, 0);
            }

            if constexpr (std::is_same_v<T, uint8_t>) {
                switch(image->channels) {
                    case 1: format = VK_FORMAT_R8_UNORM; break;
                    case 2: format = VK_FORMAT_R8G8_UNORM; break;
                    default: format = VK_FORMAT_R8G8B8A8_UNORM; break;
                }
            }
            else if constexpr (std::is_same_v<T, float>) {
                switch(image->channels) {
                    case 1: format = VK_FORMAT_R32_SFLOAT; break;
                    case 2: format = VK_FORMAT_R32G32_SFLOAT; break;
                    default: format = VK_FORMAT_R32G32B32A32_SFLOAT; break;
                }
            }
            else if constexpr (std::is_same_v<T, float16>) {
                switch(image->channels) {
                    case 1: format = VK_FORMAT_R16_SFLOAT; break;
                    case 2: format = VK_FORMAT_R16G16_SFLOAT; break;
                    default: format = VK_FORMAT_R16G16B16A16_SFLOAT; break;
                }
            }

            Reallocate(Graphics::ImageType::Image2D, image->width, image->height, 1, filtering, wrapping, dedicatedMemory, usedForRenderTarget);
            RecreateSampler(filtering, wrapping);

            SetData(image->GetData());

        }

    }

}