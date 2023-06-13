#ifndef AE_TEXTURE2D_H
#define AE_TEXTURE2D_H

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
                Filtering filtering = Filtering::Nearest);

            /**
             * Constructs a Texture2D object from an image file.
             * @param filename The filename of the image
             * @param colorSpaceConversion Whether or not a sRGB to RGB conversion should be done
             * @param filtering The filtering of the texture.
             * @param forceChannels Can be used to force a number of channels which should be loaded from the file.
             */
            explicit Texture2D(std::string filename, bool colorSpaceConversion = true,
                Filtering filtering = Filtering::Anisotropic, int32_t forceChannels = 0);

            /**
             * Constructs a Texture2D object from an image object.
             * @param image The image object.
             * @param filtering The filtering of the texture.
             */
            explicit Texture2D(Common::Image<uint8_t>& image, Filtering filtering = Filtering::Anisotropic);

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
             * @param filename The name of the file
             * @note Only uint8_t, uint16_t and float are supported typenames.
             */
            template<typename T>
            void Save(std::string filename, bool flipHorizontally = false);

        private:
            void InitializeInternal(Common::Image<uint8_t>& image, Wrapping wrapping,
                Filtering filtering);

        };

        template<typename T>
        void Texture2D::Save(std::string filename, bool flipHorizontally) {

            Common::Image<T> image(width, height, channels);

            if constexpr (std::is_same_v<T, uint8_t>) {
                image.fileFormat = AE_IMAGE_PNG;
                filename += ".png";
            }
            else if constexpr (std::is_same_v<T, uint16_t>) {
                image.fileFormat = AE_IMAGE_PGM;
                filename += ".pgm";
            }
            else if constexpr (std::is_same_v<T, float>) {
                image.fileFormat = AE_IMAGE_HDR;
                filename += ".hdr";
            }

            image.SetData(GetData<T>());

            if (flipHorizontally)
                image.FlipHorizontally();

            Loader::ImageLoader::SaveImage(image, filename);

        }



    }

}

#endif