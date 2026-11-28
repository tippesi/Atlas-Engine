#pragma once

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
              * Construct a Texture2DArray object.
              * @param width The width of the texture.
              * @param height The height of the texture.
              * @param depth The number of texture depth.
              * @param format The texture format.
              * @param wrapping The wrapping of the texture. Controls texture border behaviour.
              * @param filtering The filtering of the texture.
              */
            Texture3D(int32_t width, int32_t height, int32_t depth, VkFormat format,
                Wrapping wrapping = Wrapping::Repeat, Filtering filtering = Filtering::Nearest,
                bool dedicatedMemory = false, bool usedForRenderTarget = false);

            /**
             * Resizes the texture
             * @param width The new width of the texture.
             * @param height The new height of the texture.
             * @param depth The new depth of the texture.
             * @warning This results in a loss of texture data.
             */
            void Resize(int32_t width, int32_t height, int32_t depth);

        };

    }

}