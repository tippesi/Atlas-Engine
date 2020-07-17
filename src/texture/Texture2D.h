#ifndef AE_TEXTURE2D_H
#define AE_TEXTURE2D_H

#include "../System.h"
#include "Texture.h"

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
             * @param filename
             * @param colorSpaceConversion
             * @param anisotropicFiltering
             * @param generateMipMaps
			 * @param forceChannels
             */
            Texture2D(std::string filename, bool colorSpaceConversion = true, bool anisotropicFiltering = true,
				bool generateMipMaps = true, int32_t forceChannels = 0);

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
             * Retrieves the data of the texture from the GPU.
             * @return A vector holding the data.
             * @note Depending on the data type of that texture the stride
             * between each pixel may vary and data for one channel may be larger
             * than 8 bits. To get the size of the data type call TypeFormat::GetSize().
             */
            std::vector<uint8_t> GetData();

            /**
             * Resizes the texture
             * @param width The new width of the texture.
             * @param height The new height of the texture.
             * @warning This results in a loss of texture data and a
             * change of the texture id.
             */
            void Resize(int32_t width, int32_t height);

            /**
             * Saves the texture to a PNG image file
             * @param filename The name of the file
             * @note Only UNSIGNED_BYTE textures are supported.
             */
            void SaveToPNG(std::string filename, bool flipHorizontally = false);

        protected:
            void ReserveStorage(int32_t mipCount);

        };

    }

}

#endif