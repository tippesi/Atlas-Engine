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
             * Copies the data from another texture to the texture object.
             * @param that Another texture.
             * @return A reference to the texture.
             * @note The graphics API object will be changed.
             */
			Texture& operator=(const Texture& that);

			/**
			 * Binds the texture as a sampler to the active texture unit
			 */
			void Bind();

			/**
			 * Binds the texture as a sampler to a texture unit.
			 * @param unit The texture unit the texture should be bound to.
			 * @note The texture unit should be between GL_TEXTURE0-GL_TEXTURE_MAX
			 */
			void Bind(uint32_t unit);

			/**
			 * Binds the texture to an image binding point (specified in shader)
			 * @param access The kind of access needed e.g. GL_WRITE_ONLY
			 * @param unit The binding point specified in the shader
			 * @param level The mipmap level which should be accessed
			 * @note If the texture has multiple layers, e.g. it is a Texture2DArray
			 * then all layers will be made available.
			 */
			void Bind(uint32_t access, uint32_t unit, int32_t level = 0);

			/**
			 * Unbinds any texture from the textures target.
			 */
			void Unbind();

            /**
             * Returns the ID of the texture object.
             * @return The ID of the texture object.
             */
            uint32_t GetID();

            /**
             * Returns the data type of the texture.
             * @return The data type, e.g AE_UBYTE
             */
            uint32_t GetDataType();

            /**
             * Returns the sized format of the texture.
             * @return The sized format, e.g AE_RGB16F
             */
            int32_t GetSizedFormat();

            /**
             * Copies a texture to the texture.
             * @param texture The other texture where the data is taken from.
             */
            void Copy(const Texture& texture);

            /**
             * Copies a texture to the texture.
             * @param texture The other texture where the data is taken from.
             * @param srcX The x coordinate of the source texture to be copied from.
             * @param srcY The y coordinate of the source texture to be copied from.
             * @param srcZ The z coordinate of the source texture to be copied from.
             * @param destX The x coordinate of the destination texture to be copied to.
             * @param destY The y coordinate of the destination texture to be copied to.
             * @param destZ The z coordinate of the destination texture to be copied to.
             * @param width The width of the region to be copied.
             * @param height The height of the region to be copied.
             * @param depth The depth of the region to be copied.
             * @note The destination texture is the object where this method is called,
             * while the source texture is given to the method as the texture parameter.
			 * @warning In case one of the parameters exceeds the limit of one of the textures
			 * the method won't do anything and return before any copy operation.
             */
			void Copy(const Texture& texture, int32_t srcX, int32_t srcY,
				int32_t srcZ, int32_t destX, int32_t destY, int32_t destZ,
				int32_t width, int32_t height, int32_t depth);

			/**
			 * Generates the mipmaps.
			 */
			void GenerateMipmap();

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
             * @param baseFormat The base format, e.g AE_RGB
             * @return The number of channels.
             */
            static int32_t GetChannelCount(int32_t baseFormat);

            /**
             * Determines a suggested sized format for the current platform.
             * @param channelCount The number of channels of the image
             * @return A suggested texture format, e.g AE_RGB8
             */
            static int32_t GetSuggestedFormat(int32_t channelCount);

            /**
             * Checks the available extensions which the class might use
             */
            static void CheckExtensions();

            int32_t width = 0;
            int32_t height = 0;

            int32_t layers = 0;

            int32_t channels = 0;

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
			 * @return The flipped data.
             */
			std::vector<uint8_t> FlipDataHorizontally(std::vector<uint8_t>& data);

            /**
             * Determines the maximum needed mipmap level.
             * @return The mipmap level.
             */
            int32_t GetMipMapLevel();

            void Generate(uint32_t target, int32_t sizedFormat, int32_t wrapping,
                          int32_t filtering, bool anisotropicFiltering, bool generateMipMaps);

			void DeepCopy(const Texture& that);

            uint32_t ID = 0;

            uint32_t target = 0;

            uint32_t dataType = 0;
            int32_t sizedFormat = 0;

            int32_t wrapping = 0;
            int32_t filtering = 0;

            bool anisotropicFiltering = false;
            bool mipmaps = false;

            static int32_t anisotropyLevel;

            static bool anisotropicFilteringSupported;

        };

    }

}

#endif