#pragma once

#include "../System.h"
#include "Texture.h"

namespace Atlas {

    namespace Texture {

        class Texture2DArray : public Texture {

        public:
            /**
             * Constructs a Texture2DArray object.
             */
            Texture2DArray() = default;

            /**
              * Construct a Texture2DArray object.
              * @param width The width of the texture.
              * @param height The height of the texture.
              * @param layers The number of texture depth.
              * @param format The texture format.
              * @param wrapping The wrapping of the texture. Controls texture border behaviour.
              * @param filtering The filtering of the texture.
              */
            Texture2DArray(int32_t width, int32_t height, int32_t layers, VkFormat format,
                Wrapping wrapping = Wrapping::Repeat, Filtering filtering = Filtering::Nearest);

            /**
             * Sets the data of the texture object.
             * @param data A vector holding the new data.
             * @param layer The layer where the data should be set.
             * @param count The number of layer where the data should be written to.
             * @note The data has to have the size of count * width * height * channels.
             */
            void SetData(std::vector<uint8_t>& data, int32_t layer, int32_t count = 1);

            /**
             * Sets the data of the texture object.
             * @param data A vector holding the new data.
             * @param x Specifies a pixel offset in x direction.
             * @param y Specifies a pixel offset in y direction.
             * @param z Specifies a pixel offset in z direction.
             * @param width The width of the texture region to be set.
             * @param height The height of the texture region to be set.
             * @param layers The depth of the texture region to be set.
             */
            void SetData(std::vector<uint8_t>& data, int32_t x, int32_t y, int32_t z,
                int32_t width, int32_t height, int32_t layers);

            /**
            * Sets the data of the texture object.
            * @param data A vector holding the new data.
            * @param layer The layer where the data should be set.
            * @param count The number of layer where the data should be written to.
            * @note The data has to have the size of count * width * height * channels.
            */
            void SetData(std::vector<uint16_t>& data, int32_t layer, int32_t count = 1);

            /**
             * Sets the data of the texture object.
             * @param data A vector holding the new data.
             * @param x Specifies a pixel offset in x direction.
             * @param y Specifies a pixel offset in y direction.
             * @param z Specifies a pixel offset in z direction.
             * @param width The width of the texture region to be set.
             * @param height The height of the texture region to be set.
             * @param layers The depth of the texture region to be set.
             */
            void SetData(std::vector<uint16_t>& data, int32_t x, int32_t y, int32_t z,
                int32_t width, int32_t height, int32_t layers);

            /**
            * Sets the data of the texture object.
            * @param data A vector holding the new data.
            * @param depth The depth where the data should be set.
            * @param count The number of layer where the data should be written to.
            * @note The data has to have the size of count * width * height * channels.
            */
            void SetData(std::vector<float>& data, int32_t depth, int32_t count = 1);

            /**
             * Sets the data of the texture object.
             * @param data A vector holding the new data.
             * @param x Specifies a pixel offset in x direction.
             * @param y Specifies a pixel offset in y direction.
             * @param z Specifies a pixel offset in z direction.
             * @param width The width of the texture region to be set.
             * @param height The height of the texture region to be set.
             * @param layers The depth of the texture region to be set.
             */
            void SetData(std::vector<float>& data, int32_t x, int32_t y, int32_t z,
                int32_t width, int32_t height, int32_t layers);

            /**
             * Resizes the texture
             * @param width The new width of the texture.
             * @param height The new height of the texture.
             * @param layers The new depth of the texture.
             * @warning This results in a loss of texture data.
             */
            void Resize(int32_t width, int32_t height, int32_t layers);

            /**
             * Saves a layer of the texture to a PNG image file
             * @param filename The name of the file
             * @param layer
             */
            void SaveToPNG(std::string filename, int32_t layer);

        };

    }

}