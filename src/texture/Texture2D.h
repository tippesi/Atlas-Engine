#ifndef AE_TEXTURE2D_H
#define AE_TEXTURE2D_H

#include "../System.h"
#include "Texture.h"

#include "../common/Image.h"

namespace Atlas {

    namespace Texture {

        class Texture2D : public Texture {

        public:
			/**
			 * Constructs a Texture2D object.
			 */
            Texture2D() = default;

            /**
             * Constructs a Texture2D object.
			 * @param that Another Texture2D object.
             */
            Texture2D(const Texture2D& that);

            /**
			 * Construct a Texture2D object.
			 * @param width The width of the texture.
			 * @param height The height of the texture.
			 * @param sizedFormat The sized texture format. See {@link TextureFormat.h} for more.
			 * @param wrapping The wrapping of the texture. Controls texture border behaviour.
			 * @param filtering The filtering of the texture.
			 * @param anisotropicFiltering Whether or not anisotropic filtering is used.
			 * @param generateMipMaps Whether or not mipmap can be used. Generate using GenerateMipmap()
			 */
            Texture2D(int32_t width, int32_t height, int32_t sizedFormat, int32_t wrapping = GL_CLAMP_TO_EDGE, 
				int32_t filtering = GL_NEAREST, bool anisotropicFiltering = false, bool generateMipMaps = false);

            /**
             * Constructs a Texture2D object from an image file.
             * @param filename The filename of the image
             * @param colorSpaceConversion Whether or not a sRGB to RGB conversion should be done
             * @param anisotropicFiltering Whether or not anisotropic filtering is used.
			 * @param generateMipMaps Whether or not mipmap can be used. Generate using GenerateMipmap()
			 * @param forceChannels Can be used to force a number of channels which should be loaded from the file.
             */
            explicit Texture2D(std::string filename, bool colorSpaceConversion = true, bool anisotropicFiltering = true,
				bool generateMipMaps = true, int32_t forceChannels = 0);

			/**
			 * Constructs a Texture2D object from an image object.
			 * @param image The image object.
			 * @param anisotropicFiltering Whether or not anisotropic filtering is used.
			 * @param generateMipMaps Whether or not mipmap can be used. Generate using GenerateMipmap()
			 */
			explicit Texture2D(Common::Image<uint8_t>& image, bool anisotropicFiltering = true, 
				bool generateMipMaps = true);

            /**
             * Copies the data from another texture to the texture object.
             * @param that Another texture.
             * @return A reference to the texture.
             * @note The graphics API object will be changed.
             */
            Texture2D& operator=(const Texture2D& that);

			/**
			 * Sets the data of the texture
			 * @param data A vector holding the new data.
			 */
			void SetData(std::vector<uint8_t>& data);

            /**
             * Sets the data of the texture
             * @param data A vector holding the new data.
			 * @param mipLevel The mipmap level
             */
            void SetData(std::vector<uint8_t>& data, int32_t mipLevel);

			/**
			 * Sets the data of the texture
			 * @param data A pointer to the data
			 */
			void SetData(std::vector<uint16_t>& data);

            /**
             * Sets the data of the texture
             * @param data A pointer to the data
			 * @param mipLevel The mipmap level
             */
            void SetData(std::vector<uint16_t>& data, int32_t mipLevel);

            /**
             * Sets the data of the texture
             * @param data A pointer to the data
             */
            void SetData(std::vector<float16>& data);

			/**
			 * Sets the data of the texture
			 * @param data A pointer to the data
			 */
			void SetData(std::vector<float>& data);

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

        protected:
            void ReserveStorage(int32_t mipCount) override;

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