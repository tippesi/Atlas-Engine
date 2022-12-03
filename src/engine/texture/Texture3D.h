#ifndef AE_TEXTURE3D_H
#define AE_TEXTURE3D_H

#include "Texture.h"

namespace Atlas {

    namespace Texture {

        class Texture3D : public Texture {

        public:
            /**
			 * Constructs a Texture2DArray object.
			 */
            Texture3D() = default;

            /**
             * Constructs a Texture2DArray object.
             * @param that Another Texture2DArray object.
             */
            Texture3D(const Texture3D& that);

            /**
              * Construct a Texture2DArray object.
              * @param width The width of the texture.
              * @param height The height of the texture.
              * @param depth The number of texture depth.
              * @param sizedFormat The sized texture format. See {@link TextureFormat.h} for more.
              * @param wrapping The wrapping of the texture. Controls texture border behaviour.
              * @param filtering The filtering of the texture.
              */
            Texture3D(int32_t width, int32_t height, int32_t depth, int32_t sizedFormat,
                      int32_t wrapping = GL_CLAMP_TO_EDGE, int32_t filtering = GL_NEAREST,
                      bool anisotropicFiltering = false, bool generateMipMaps = false);

            /**
			 * Copies the data from another texture to the texture object.
			 * @param that Another texture.
			 * @return A reference to the texture.
			 * @note The graphics API object will be changed.
			 */
            Texture3D& operator=(const Texture3D& that);

            /**
             * Resizes the texture
             * @param width The new width of the texture.
             * @param height The new height of the texture.
             * @param depth The new depth of the texture.
             * @warning This results in a loss of texture data.
             */
            void Resize(int32_t width, int32_t height, int32_t depth);

        protected:
            void ReserveStorage(int32_t mipCount) override;

        };

    }

}

#endif