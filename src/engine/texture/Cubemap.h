#pragma once

#include "System.h"
#include "Texture.h"

#include <vector>

namespace Atlas {

    namespace Texture {

        class Cubemap : public Texture {

        public:
            /**
             * Constructs a Cubemap object.
             */
            Cubemap() = default;

            /**
             * Constructs a Cubemap object.
             * @param right
             * @param left
             * @param top
             * @param bottom
             * @param front
             * @param back
             * @note All textures are expected to have the same dimensions
             */
            Cubemap(std::string right, std::string left, std::string top,
                    std::string bottom, std::string front, std::string back);

            /**
            * Constructs a Cubemap object.
            * @param filename
            * @param resolution
            * @note The file has to be a equirectangular map.
            */
            Cubemap(std::string filename);

            /**
             * Construct a Cubemap object.
             * @param width The width of the texture.
             * @param height The height of the texture.
             * @param format The texture format.
             * @param wrapping The wrapping of the texture. Controls texture border behaviour.
             * @param filtering The filtering of the texture.
             */
            Cubemap(int32_t width, int32_t height, VkFormat format,
                Wrapping wrapping = Wrapping::ClampToEdge, Filtering filtering = Filtering::Linear);

            /**
             * Sets the data of the texture
             * @param data A vector holding the new data.
             * @param layer The layer where the data should be set.
             * @note The data has to have the size of width * height * channels.
             */
            void SetData(std::vector<uint8_t>& data, int32_t layer);

            /**
             * Sets the data of the texture
             * @param data A vector holding the new data.
             * @param layer The layer where the data should be set.
             * @note The data has to have the size of width * height * channels.
             */
            void SetData(std::vector<float>& data, int32_t layer);

            /**
             * Sets the data of the texture
             * @param data A vector holding the new data.
             * @param layer The layer where the data should be set.
             * @note The data has to have the size of width * height * channels.
             */
            void SetData(std::vector<float16>& data, int32_t layer);

            /**
             *
             * @param layer
             * @return
             */
            std::vector<uint8_t> GetData(int32_t layer);

        };

    }

}