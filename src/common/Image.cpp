#include "Image.h"

namespace Atlas {

	namespace Common {

		Image8::Image8(int32_t width, int32_t height, int32_t channels) :
			Image(width, height, channels) {

			data.resize((size_t)width * height * channels);

		}

		ivec4 Image8::Sample(int32_t x, int32_t y) {

			if (x < 0 || y < 0 || x >= width || y >= height)
				return ivec4(0);

			ivec4 result(0);

			int32_t index = y * width + x;

			for (int32_t i = 0; i < channels; i++)
				result[i] = (int32_t)data[(size_t)index + i];

			return result;

		}

		ivec4 Image8::Sample(float x, float y) {

			auto fwidth = (float)width;
			auto fheight = (float)height;

			x *= fwidth;
			y *= fheight;

			if (x < 0.0f || y < 0.0f ||
				x >= fwidth || y >= fheight)
				return ivec4(0);

			ivec4 result(0);

			int32_t index = (int32_t)y * width + (int32_t)x;

			for (int32_t i = 0; i < channels; i++)
				result[i] = (int32_t)data[(size_t)index + i];

			return result;

		}

		ivec4 Image8::SampleLinear(float x, float y) {

			auto fwidth = (float)width;
			auto fheight = (float)height;

			x *= fwidth;
			y *= fheight;

			if (x < 0.0f || y < 0.0f ||
				x >= fwidth || y >= fheight)
				return ivec4(0);

			auto localX = x - floorf(x);
			auto localY = y - floorf(y);

			ivec4 result(0);

			int32_t index = (int32_t)y * width + (int32_t)x;

			for (int32_t i = 0; i < channels; i++)
				result[i] = (int32_t)data[index + i];

			return result;

		}

		Image16::Image16(int32_t width, int32_t height, int32_t channels) :
			Image(width, height, channels) {

			data.resize((size_t)width * height * channels);

		}

		ivec4 Image16::Sample(int32_t x, int32_t y) {

			if (x < 0 || y < 0 || x >= width || y >= height)
				return ivec4(0);

			ivec4 result(0);

			int32_t index = y * width + x;

			for (int32_t i = 0; i < channels; i++)
				result[i] = (int32_t)data[(size_t)index + i];

			return result;

		}

		ivec4 Image16::Sample(float x, float y) {

			auto fwidth = (float)width;
			auto fheight = (float)height;

			x *= fwidth;
			y *= fheight;

			if (x < 0.0f || y < 0.0f ||
				x >= fwidth || y >= fheight)
				return ivec4(0);

			ivec4 result(0);

			int32_t index = (int32_t)y * width + (int32_t)x;

			for (int32_t i = 0; i < channels; i++)
				result[i] = (int32_t)data[(size_t)index + i];

			return result;

		}

	}

}