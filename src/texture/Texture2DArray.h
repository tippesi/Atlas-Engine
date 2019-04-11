#ifndef AE_TEXTURE2DARRAY_H
#define AE_TEXTURE2DARRAY_H

#include "../System.h"
#include "Texture.h"

namespace Atlas {

    namespace Texture {

        class Texture2DArray : public Texture {

        public:
			/**
			 * Constructs a Texture2DArray object.
			 */
			Texture2DArray() {}

            /**
             * Construct a Texture2DArray object.
             * @param width
             * @param height
             * @param depth
             * @param sizedFormat
             * @param wrapping
             * @param filtering
             * @param anisotropicFiltering
             * @param generateMipMaps
             */
            Texture2DArray(int32_t width, int32_t height, int32_t depth, int32_t sizedFormat,
                           int32_t wrapping, int32_t filtering, bool anisotropicFiltering, bool generateMipMaps);

			/**
			 * Copies the data from another texture to the texture object.
			 * @param that Another texture.
			 * @return A reference to the texture.
			 * @note The graphics API object will be changed.
			 */
			Texture2DArray& operator=(const Texture2DArray& that);

            /**
             * Sets the data of the texture
             * @param data A vector holding the new data.
             * @param depth The depth where the data should be set.
             * @param count The number of layer where the data should be written to.
             * @note The data has to have the size of count * width * height * channels.
             */
            void SetData(std::vector<uint8_t>& data, int32_t depth, int32_t count = 1);

            /**
            * Sets the data of the texture
            * @param data A vector holding the new data.
            * @param depth The depth where the data should be set.
            * @param count The number of layer where the data should be written to.
            * @note The data has to have the size of count * width * height * channels.
            */
            void SetData(std::vector<uint16_t>& data, int32_t depth, int32_t count = 1);

            /**
             * Retrieves the data of the texture from the GPU.
             * @param depth The depth where the data should be retrieved.
             * @return A vector holding the data.
             */
            std::vector<uint8_t> GetData(int32_t depth);

            /**
             * Resizes the texture
             * @param width The new width of the texture.
             * @param height The new height of the texture.
             * @param depth The new depth of the texture.
             * @warning This results in a loss of texture data.
             */
            void Resize(int32_t width, int32_t height, int32_t depth);

            /**
             * Saves a layer of the texture to a PNG image file
             * @param filename The name of the file
             * @param depth This results in a loss of texture data and a
             * change of the texture id.
             */
            void SaveToPNG(std::string filename, int32_t depth);

        protected:
            void ReserveStorage(int32_t mipCount);

        };

    }

}

#endif