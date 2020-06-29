#include "Image.h"
#include "libraries/stb/stb_image_resize.h"

namespace Atlas {

	namespace Common {

		Image8::Image8(int32_t width, int32_t height, int32_t channels) :
			Image(width, height, channels) {

			Resize(width, height);

		}

		void Image8::SetData(std::vector<uint8_t>& data) {

			this->data[0] = data;

		}

		void Image8::SetData(int32_t x, int32_t y, int32_t channel, uint8_t data) {

			int32_t index = (y * width + x) * channels;

			this->data[0][index + channel] = data;

		}

		std::vector<uint8_t>& Image8::GetData(int32_t mipLevel) {

			return data[mipLevel];

		}

		std::vector<uint8_t> Image8::GetChannelData(int32_t channelOffset,
			int32_t channelCount, int32_t mipLevel) {

			std::vector<uint8_t> channelData;

			auto totalPixels = (int32_t)data[mipLevel].size() / channels;
			auto count = channelOffset + channelCount;

			for (int32_t i = 0; i < totalPixels; i++) {
				for (int32_t j = channelOffset; j < count; j++) {
					channelData.push_back(data[mipLevel][i * channels + j]);
				}
			}

			return channelData;

		}

		int32_t Image8::GetMipmapLevelCount() {

			return (int32_t)data.size();

		}

		ivec4 Image8::Sample(int32_t x, int32_t y, int32_t mipLevel) {

			auto width = resolutions[mipLevel].x;
			auto height = resolutions[mipLevel].y;

			x = x < 0 ? 0 : x;
			x = x >= width ? width - 1 : x;

			y = y < 0 ? 0 : y;
			y = y >= height ? height - 1 : y;

			ivec4 result(0);

			auto index = (y * width + x) * channels;

			for (int32_t i = 0; i < channels; i++)
				result[i] = (int32_t)data[mipLevel][(size_t)index + i];

			return result;

		}

		ivec4 Image8::Sample(float x, float y, int32_t mipLevel) {

			auto fWidth = (float)resolutions[mipLevel].x;
			auto fHeight = (float)resolutions[mipLevel].y;

			x *= fWidth;
			y *= fHeight;

			x = glm::clamp(x, 0.0f, fWidth - 1.0f);
			y = glm::clamp(y, 0.0f, fHeight - 1.0f);

			ivec4 result(0);

			auto index = (int32_t)(y * fWidth + x) * channels;

			for (int32_t i = 0; i < channels; i++)
				result[i] = (int32_t)data[mipLevel][(size_t)index + i];

			return result;

		}

		ivec4 Image8::SampleBilinear(float x, float y, int32_t mipLevel) {

			auto fWidth = (float)resolutions[mipLevel].x;
			auto fHeight = (float)resolutions[mipLevel].y;

			x *= fWidth;
			y *= fHeight;

			auto ox = (int32_t)x;
			auto oy = (int32_t)y;

			x = x - floorf(x);
			y = y - floorf(y);

			// Fetch the four texture samples
			auto q00 = (vec4)Sample(ox, oy, mipLevel);
			auto q10 = (vec4)Sample(ox + 1, oy, mipLevel);
			auto q01 = (vec4)Sample(ox, oy + 1, mipLevel);
			auto q11 = (vec4)Sample(ox + 1, oy + 1, mipLevel);

			// Interpolate samples horizontally
			auto h0 = (1.0f - x) * q00 + x * q10;
			auto h1 = (1.0f - x) * q01 + x * q11;

			// Interpolate samples vertically
			return (ivec4)((1.0f - y) * h0 + y * h1);

		}

		void Image8::GenerateMipmap() {

			auto width = this->width;
			auto height = this->height;

			// Max mipmap level count
			auto mipLevel = (size_t)floor(log2(glm::max((float)width,
				(float)height))) + 1;

			data.resize(mipLevel);
			resolutions.resize(mipLevel);

			for (int32_t i = 0; i < data.size(); i++) {
				resolutions[i] = ivec2(width, height);
				data[i].resize(width * height * channels);
				width /= 2;
				height /= 2;
			}

			for (int32_t i = 1; i < data.size(); i++) {
				width /= 2;
				height /= 2;
				stbir_resize_uint8_generic(data[i-1].data(), width * 2,
					height * 2, width * 2 * channels, 
					data[i].data(), width,
					height, width * channels, channels, -1, 0,
					STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, 
					STBIR_COLORSPACE_LINEAR, nullptr);
			}

		}

		void Image8::Resize(int32_t width, int32_t height) {

			std::vector<uint8_t> resizableData;

			if (data.size()) {
				resizableData = data[0];
			}

			data.resize(1);
			resolutions.resize(1);

			data[0].resize(width * height * channels);
			resolutions[0] = ivec2(width, height);

			if (resizableData.size()) {
				stbir_resize_uint8_generic(resizableData.data(), this->width,
					this->height, this->width * channels,
					data[0].data(), width,
					height, width * channels, channels, -1, 0,
					STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT,
					STBIR_COLORSPACE_LINEAR, nullptr);
			}

			this->width = width;
			this->height = height;

		}

		void Image8::ApplyFilter(Filter filter) {

			auto tmp = *this;

			if (filter.IsSeparable()) {

				std::vector<float> weights;
				std::vector<float> offsets;

				filter.GetLinearized(&weights, &offsets, false);

				// Horizontal pass
				for (int32_t y = 0; y < height; y++) {
					for (int32_t x = 0; x < width; x++) {

						vec4 color = vec4(0.0f);

						for (int32_t i = 0; i < weights.size(); i++) {
							int32_t off = x + (int32_t)offsets[i];
							color += (vec4)Sample(off, y) * weights[i];
						}

						tmp.SetData(x, y, 0, (uint8_t)glm::round(color.x));
						tmp.SetData(x, y, 1, (uint8_t)glm::round(color.y));
						tmp.SetData(x, y, 2, (uint8_t)glm::round(color.z));
						tmp.SetData(x, y, 3, (uint8_t)glm::round(color.w));

					}
				}

				// Vertical pass
				for (int32_t y = 0; y < height; y++) {
					for (int32_t x = 0; x < width; x++) {

						vec4 color = vec4(0.0f);

						for (int32_t i = 0; i < weights.size(); i++) {
							int32_t off = y + (int32_t)offsets[i];
							color += (vec4)tmp.Sample(x, off) * weights[i];
						}

						SetData(x, y, 0, (uint8_t)glm::round(color.x));
						SetData(x, y, 1, (uint8_t)glm::round(color.y));
						SetData(x, y, 2, (uint8_t)glm::round(color.z));
						SetData(x, y, 3, (uint8_t)glm::round(color.w));

					}
				}

			}
			else {

				std::vector<std::vector<float>> weights;
				std::vector<std::vector<ivec2>> offsets;

				filter.Get(&weights, &offsets);

				for (int32_t y = 0; y < height; y++) {
					for (int32_t x = 0; x < width; x++) {

						vec4 color = vec4(0.0f);

						for (uint32_t i = 0; i < weights.size(); i++) {
							for (uint32_t j = 0; j < weights.size(); j++) {
								int32_t offX = x + offsets[i][j].x;
								int32_t offY = y + offsets[i][j].y;
								color += (vec4)tmp.Sample(offX, offY) * weights[i][j];
							}
						}

						SetData(x, y, 0, (uint8_t)glm::round(color.x));
						SetData(x, y, 1, (uint8_t)glm::round(color.y));
						SetData(x, y, 2, (uint8_t)glm::round(color.z));
						SetData(x, y, 3, (uint8_t)glm::round(color.w));

					}
				}

			}

		}

		Image16::Image16(int32_t width, int32_t height, int32_t channels) :
			Image(width, height, channels) {

			data.resize((size_t)width * height * channels);

		}

		void Image16::SetData(std::vector<uint16_t>& data) {

			this->data = data;

		}

		void Image16::SetData(int32_t x, int32_t y, int32_t channel, uint16_t data) {

			int32_t index = (y * width + x) * channels;

			this->data[index + channel] = data;

		}

		std::vector<uint16_t>& Image16::GetData() {

			return data;

		}

		ivec4 Image16::Sample(int32_t x, int32_t y) {

			auto width = this->width;
			auto height = this->height;

			x = x < 0 ? 0 : x;
			x = x >= width ? width - 1 : x;

			y = y < 0 ? 0 : y;
			y = y >= height ? height - 1 : y;

			ivec4 result(0);

			auto index = (y * width + x) * channels;

			for (int32_t i = 0; i < channels; i++)
				result[i] = (int32_t)data[(size_t)index + i];

			return result;

		}

		ivec4 Image16::Sample(float x, float y) {

			auto fWidth = (float)this->width;
			auto fHeight = (float)this->height;

			x *= fWidth;
			y *= fHeight;

			x = glm::clamp(x, 0.0f, fWidth - 1.0f);
			y = glm::clamp(y, 0.0f, fHeight - 1.0f);

			ivec4 result(0);

			auto index = (int32_t)(y * fWidth + x) * channels;

			for (int32_t i = 0; i < channels; i++)
				result[i] = (int32_t)data[(size_t)index + i];

			return result;

		}

		ivec4 Image16::SampleBilinear(float x, float y) {

			auto fWidth = (float)this->width;
			auto fHeight = (float)this->height;

			x *= fWidth;
			y *= fHeight;

			auto ox = (int32_t)x;
			auto oy = (int32_t)y;

			x = x - floorf(x);
			y = y - floorf(y);

			// Fetch the four texture samples
			auto q00 = (vec4)Sample(ox, oy);
			auto q10 = (vec4)Sample(ox + 1, oy);
			auto q01 = (vec4)Sample(ox, oy + 1);
			auto q11 = (vec4)Sample(ox + 1, oy + 1);

			// Interpolate samples horizontally
			auto h0 = (1.0f - x) * q00 + x * q10;
			auto h1 = (1.0f - x) * q01 + x * q11;

			// Interpolate samples vertically
			return (ivec4)((1.0f - y) * h0 + y * h1);

		}

		ImageFloat::ImageFloat(int32_t width, int32_t height, int32_t channels) :
			Image(width, height, channels) {

			data.resize((size_t)width * height * channels);

		}

		void ImageFloat::SetData(std::vector<float>& data) {

			this->data = data;

		}

		void ImageFloat::SetData(int32_t x, int32_t y, int32_t channel, float data) {

			int32_t index = (y * width + x) * channels;

			this->data[index + channel] = data;

		}

		std::vector<float>& ImageFloat::GetData() {

			return data;

		}

		vec4 ImageFloat::Sample(int32_t x, int32_t y) {

			auto width = this->width;
			auto height = this->height;

			x = x < 0 ? 0 : x;
			x = x >= width ? width - 1 : x;

			y = y < 0 ? 0 : y;
			y = y >= height ? height - 1 : y;

			vec4 result(0.0f);

			auto index = (y * width + x) * channels;

			for (int32_t i = 0; i < channels; i++)
				result[i] = data[(size_t)index + i];

			return result;

		}

		vec4 ImageFloat::Sample(float x, float y) {

			auto fWidth = (float)this->width;
			auto fHeight = (float)this->height;

			x *= fWidth;
			y *= fHeight;

			x = glm::clamp(x, 0.0f, fWidth - 1.0f);
			y = glm::clamp(y, 0.0f, fHeight - 1.0f);

			vec4 result(0.0f);

			auto index = (int32_t)(y * fWidth + x) * channels;

			for (int32_t i = 0; i < channels; i++)
				result[i] = data[(size_t)index + i];

			return result;

		}

		vec4 ImageFloat::SampleBilinear(float x, float y) {

			auto fWidth = (float)this->width;
			auto fHeight = (float)this->height;

			x *= fWidth;
			y *= fHeight;

			auto ox = (int32_t)x;
			auto oy = (int32_t)y;

			x = x - floorf(x);
			y = y - floorf(y);

			// Fetch the four texture samples
			auto q00 = Sample(ox, oy);
			auto q10 = Sample(ox + 1, oy);
			auto q01 = Sample(ox, oy + 1);
			auto q11 = Sample(ox + 1, oy + 1);

			// Interpolate samples horizontally
			auto h0 = (1.0f - x) * q00 + x * q10;
			auto h1 = (1.0f - x) * q01 + x * q11;

			// Interpolate samples vertically
			return (1.0f - y) * h0 + y * h1;

		}

	}

}