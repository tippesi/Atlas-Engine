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
             * @param dataType
             * @param width
             * @param height
             * @param sizedFormat
             * @param wrapping
             * @param filtering
             * @param anisotropicFiltering
             * @param generateMipMaps
             */
            Texture2D(GLenum dataType, int32_t width, int32_t height, int32_t sizedFormat,
                      int32_t wrapping, int32_t filtering, bool anisotropicFiltering, bool generateMipMaps);

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
             * Binds the texture to a texture unit
             * @param unit The texture unit the texture should be bound to.
             * @note The texture unit should be between GL_TEXTURE0-GL_TEXTURE_MAX
             */
            void Bind(uint32_t unit);

            /**
             * Unbinds any texture.
             */
            void Unbind();

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
            void SaveToPNG(std::string filename);

        protected:
            void ReserveStorage(int32_t mipCount);

        };

    }

}

#endif