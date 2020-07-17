#ifndef AE_IMAGE_H
#define AE_IMAGE_H

#include "../System.h"
#include "../Filter.h"

#include <vector>

/**
 * Image file formats
 */
#define AE_IMAGE_PNG 0
#define AE_IMAGE_JPG 1
#define AE_IMAGE_BMP 2
#define AE_IMAGE_PGM 3
#define AE_IMAGE_HDR 4

namespace Atlas {

	namespace Common {

		/**
		 * Base class for the image classes.
		 */
		class Image {

		public:
			Image() = default;

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
			Image8() = default;

			Image8(int32_t width, int32_t height, int32_t channels);

			void SetData(std::vector<uint8_t>& data);

			void SetData(int32_t x, int32_t y, int32_t channel, uint8_t data);

			std::vector<uint8_t>& GetData(int32_t mipLevel = 0);

			std::vector<uint8_t> GetChannelData(int32_t channelOffset, 
				int32_t channelCount, int32_t mipLevel = 0);

			int32_t GetMipmapLevelCount();

			ivec4 Sample(int32_t x, int32_t y, int32_t mipLevel = 0);

			ivec4 Sample(float x, float y, int32_t mipLevel = 0);

			ivec4 SampleBilinear(float x, float y, int32_t mipLevel = 0);

			void GenerateMipmap();

			void Resize(int32_t width, int32_t height);

			void ApplyFilter(Filter filter);

		private:
			std::vector<std::vector<uint8_t>> data;
			std::vector<ivec2> resolutions;

		};

		/**
		 * Represents an image with 16 bits per channel.
		 */
		class Image16 : public Image {

		public:
			Image16() = default;

			Image16(int32_t width, int32_t height, int32_t channels);

			void SetData(std::vector<uint16_t>& data);

			void SetData(int32_t x, int32_t y, int32_t channel, uint16_t data);

			std::vector<uint16_t>& GetData();

			ivec4 Sample(int32_t x, int32_t y);

			ivec4 Sample(float x, float y);

			ivec4 SampleBilinear(float x, float y);

		private:
			std::vector<uint16_t> data;

		};

		/**
		 * Represents an image with 32 float bits per channel.
		 */
		class ImageFloat : public Image {

		public:
			ImageFloat() {}

			ImageFloat(int32_t width, int32_t height, int32_t channels);

			void SetData(std::vector<float>& data);

			void SetData(int32_t x, int32_t y, int32_t channel, float data);

			std::vector<float>& GetData();

			vec4 Sample(int32_t x, int32_t y);

			vec4 Sample(float x, float y);

			vec4 SampleBilinear(float x, float y);

		private:
			std::vector<float> data;

		};

	}

}

#endif