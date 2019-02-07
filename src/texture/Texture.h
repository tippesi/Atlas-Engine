#ifndef AE_TEXTURE_H
#define AE_TEXTURE_H

#include "../System.h"
#include "TextureFormat.h"

#include <vector>

namespace Atlas {

    namespace Texture {

        /**
 * The base class of all texture classes.
 */
        class Texture {

        public:
            /**
             * Creates a OpenGL texture object.
             */
            Texture();

            /**
             * Destructs the Texture object.
             */
            virtual ~Texture();

            /**
             * Returns the ID of the texture object.
             * @return The ID of the texture object.
             */
            uint32_t GetID();

            /**
             * Returns the data type of the texture.
             * @return The data type, e.g GL_UNSIGNED_BYTE
             */
            uint32_t GetDataType();

            /**
             * Returns the sized format of the texture.
             * @return The sized format, e.g GL_RGB16F
             */
            int32_t GetSizedFormat();

            /**
             * Binds the texture to a texture unit
             * @param unit The texture unit the texture should be bound to.
             * @note The texture unit should be between GL_TEXTURE0-GL_TEXTURE_MAX
             */
            virtual void Bind(uint32_t unit) = 0;

            /**
             * Unbinds any texture.
             */
            virtual void Unbind() = 0;

            /**
             * Determines the maximum anisotropy level offered by the system.
             * @return The maximum anisotropy level
             */
            static int32_t GetMaxAnisotropyLevel();

            /**
             * Sets the anisotropy level that all textures use.
             * @param anisotropyLevel
             * @note The anisotropy level will be clamped between 1
             * and GetMaxAnisotropyLevel().
             */
            static void SetAnisotropyLevel(int32_t anisotropyLevel);

            /**
             * Converts image data from gamma to linear color space.
             * @param data The data to be converted
             * @param width The width of the data
             * @param height The height of the data
             * @param channels The number of channels the data has
             */
            static void GammaToLinear(uint8_t* data, int32_t width, int32_t height, int32_t channels);

            /**
            * Converts image data from gamma to linear color space.
            * @param data The data to be converted
            * @param width The width of the data
            * @param height The height of the data
            * @param channels The number of channels the data has
            */
            static void GammaToLinear(uint16_t* data, int32_t width, int32_t height, int32_t channels);

            /**
             * Determines the number of channels for a given base format.
             * @param baseFormat The base format, e.g GL_RGB
             * @return The number of channels.
             */
            static int32_t GetChannelCount(int32_t baseFormat);

            /**
             * Determines a suggested sized format for the current platform.
             * @param channelCount The number of channels of the image
             * @return A suggested texture format, e.g GL_RGB8
             */
            static int32_t GetSuggestedFormat(int32_t channelCount);

            /**
             * Checks the available extensions which the class might use
             */
            static void CheckExtensions();

            int32_t width;
            int32_t height;

            int32_t depth;

            int32_t channels;

        protected:
            /**
             * Reserves the texture storage
             * @param mipCount The required mipmap count
             * @note Should reserve storage with glTexStorageXD
             */
            virtual void ReserveStorage(int32_t mipCount) = 0;

            /**
             * Flips texture data horizontally.
             * @param data The data to be flipped.
             */
            void FlipDataHorizontally(std::vector<uint8_t>& data);

            /**
             * Determines the maximum needed mipmap level.
             * @return The mipmap level.
             */
            int32_t GetMipMapLevel();

            void Generate(GLenum target, GLenum dataType, int32_t sizedFormat, int32_t wrapping,
                          int32_t filtering, bool anisotropicFiltering, bool generateMipMaps);

            uint32_t ID;

            uint32_t dataType;
            int32_t sizedFormat;

            int32_t wrapping;
            int32_t filtering;

            bool anisotropicFiltering;
            bool mipmaps;

            static int32_t anisotropyLevel;

            static bool anisotropicFilteringSupported;

        };

    }

}

#endif