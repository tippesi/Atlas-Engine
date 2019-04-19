#ifndef AE_CUBEMAP_H
#define AE_CUBEMAP_H

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
			Cubemap() {}

			/**
             * Constructs a Cubemap object.
			 * @param that Another Cubemap object.
             */
			Cubemap(const Cubemap& that);

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
             * @param width
             * @param height
             * @param sizedFormat
             * @param wrapping
             * @param filtering
             * @param generateMipmaps
             */
            Cubemap(int32_t width, int32_t height, int32_t sizedFormat,
                    int32_t wrapping, int32_t filtering, bool generateMipmaps);

			/**
             * Copies the data from another Cubemap object to the Cubemap object.
             * @param that Another Cubemap object.
             * @return A reference to the texture.
             * @note The graphics API object will be changed.
             */
            Cubemap& operator=(const Cubemap &that);

			/**
			 * Sets the data of the texture
			 * @param data A vector holding the new data.
			 * @param layer The layer where the data should be set.
			 * @note The data has to have the size of width * height * channels.
			 */
			void SetData(std::vector<uint8_t>& data, int32_t layer);

			std::vector<uint8_t> GetData(int32_t layer);

		protected:
			void ReserveStorage(int32_t mipCount);

        };

    }

}

#endif