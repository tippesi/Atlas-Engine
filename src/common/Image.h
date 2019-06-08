#ifndef AE_IMAGE_H
#define AE_IMAGE_H

#include "../System.h"

#include <vector>

/**
 * Image file formats
 */
#define AE_IMAGE_PNG 0
#define AE_IMAGE_JPG 1
#define AE_IMAGE_BMP 2
#define AE_IMAGE_PGM 3

namespace Atlas {

	namespace Common {

		/**
		 * Base class for the image classes.
		 */
		class Image {

		public:
			Image() {}

			Image(int32_t width, int32_t height, int32_t channels) :
				width(width), height(height), channels(channels) {}

			int32_t width = 0;
			int32_t height = 0;
			int32_t channels = 0;

			int32_t fileFormat = 0;

		};

		/**
		 * Represents an image with 8 bits per channel.
		 */
		class Image8 : public Image {

		public:
			Image8() {}

			Image8(int32_t width, int32_t height, int32_t channels);

			ivec4 Sample(int32_t x, int32_t y);

			ivec4 Sample(float x, float y);

			ivec4 SampleLinear(float x, float y);

			std::vector<uint8_t> data;

		};

		/**
		 * Represents an image with 16 bits per channel.
		 */
		class Image16 : public Image {

		public:
			Image16() {}

			Image16(int32_t width, int32_t height, int32_t channels);

			ivec4 Sample(int32_t x, int32_t y);

			ivec4 Sample(float x, float y);

			// TODO: ivec4 SampleLinear(float x, float y);

			std::vector<uint16_t> data;

		};

	}

}

#endif