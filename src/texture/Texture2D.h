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
			Texture2D() {}

            /**
             * Constructs a Texture2D object.
			 * @param that Another Texture2D object.
             */
			Texture2D(const Texture2D& that);

            /**
             * Constructs a Texture2D object.
             * @param width
             * @param height
             * @param sizedFormat
             * @param wrapping
             * @param filtering
             * @param anisotropicFiltering
             * @param generateMipMaps
             */
            Texture2D(int32_t width, int32_t height, int32_t sizedFormat, int32_t wrapping = GL_CLAMP_TO_EDGE, 
				int32_t filtering = GL_NEAREST, bool anisotropicFiltering = false, bool generateMipMaps = false);

            /**
             * Constructs a Texture2D object from an image file.
             * @param filename
             * @param colorSpaceConversion
             * @param anisotropicFiltering
             * @param generateMipMaps
             */
            Texture2D(std::string filename, bool colorSpaceConversion = true, bool anisotropicFiltering = true,
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
             * @param data A pointer to the data
             */
            void SetData(std::vector<uint16_t>& data);

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