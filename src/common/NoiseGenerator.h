#ifndef AE_NOISEGENERATOR_H
#define AE_NOISEGENERATOR_H

#include "../System.h"
#include "../texture/Texture2D.h"
#include "Image.h"

namespace Atlas {

	namespace Common {

		class NoiseGenerator {

		public:
			static void GenerateUniformNoise2D(Image<uint8_t>& image);
			static void GenerateUniformNoise2D(Image<uint16_t>& image);

			static void GeneratePerlinNoise2D(Image<uint8_t>& image, std::vector<float> amplitudes,
				uint32_t seed, float exp = 1.0f);
			static void GeneratePerlinNoise2D(Image<uint16_t>& image, std::vector<float> amplitudes,
				uint32_t seed, float exp = 1.0f);

		};		

	}

}

#endif