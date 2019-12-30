#include "NoiseGenerator.h"

#include <libraries/perlin noise/PerlinNoise.h>
#include <vector>

namespace Atlas {

	namespace Common {

		void NoiseGenerator::GenerateUniformNoise2D(Image8& image) {

			auto& data = image.GetData();

			for (size_t i = 0; i < data.size(); i++) {
				data[i] = rand() % 255;
			}

		}

		void NoiseGenerator::GenerateUniformNoise2D(Image16& image) {

			auto& data = image.GetData();

			for (size_t i = 0; i < data.size(); i++) {
				data[i] = rand() % 255;
			}

		}

		void NoiseGenerator::GeneratePerlinNoise2D(Image8& image, std::vector<float> amplitudes,
			uint32_t seed, float exp) {

			const PerlinNoise noise(seed);

			auto& data = image.GetData();

			auto amplitude = 0.0f;

			for (auto ampli : amplitudes)
				amplitude += ampli;

			amplitude = 1.0f / amplitude;

			for (size_t y = 0; y < image.height; y++) {
				for (size_t x = 0; x < image.width; x++) {
					auto fx = (float)x / (float)image.width;
					auto fy = (float)y / (float)image.height;
					auto value = noise.octaveNoise((float)fx, (float)fy, amplitudes) *
						amplitude;
					value = powf(0.5f * value + 0.5f, exp);
					for (size_t channel = 0; channel < image.channels; channel++) {
						data[(y * image.width + x) * image.channels + channel] =
							(uint8_t)(255.0f * value);
					}
				}
			}

		}

		void NoiseGenerator::GeneratePerlinNoise2D(Image16& image, std::vector<float> amplitudes, 
			uint32_t seed, float exp) {

			const PerlinNoise noise(seed);

			auto& data = image.GetData();

			auto amplitude = 0.0f;

			for (auto ampli : amplitudes)
				amplitude += ampli;

			amplitude = 1.0f / amplitude;

			for (size_t y = 0; y < image.height; y++) {
				for (size_t x = 0; x < image.width; x++) {		
					auto fx = (float)x / (float)image.width;
					auto fy = (float)y / (float)image.height;
					auto value = noise.octaveNoise((float)fx, (float)fy, amplitudes) *
						amplitude;
					value = powf(0.5f * value + 0.5f, exp);
					for (size_t channel = 0; channel < image.channels; channel++) {
						data[(y * image.width + x) * image.channels + channel] =
							(uint16_t)(65535.0f * value);
					}
				}
			}

		}

	}

}