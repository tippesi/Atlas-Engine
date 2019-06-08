#ifndef AE_OCEANSIMULATION_H
#define AE_OCEANSIMULATION_H

#include "../System.h"
#include "../renderer/helper/NoiseGenerator.h"
#include "../texture/Texture2D.h"
#include "../shader/Shader.h"

#include <vector>

namespace Atlas {

	namespace Ocean {

		class OceanSimulation {

		public:
			OceanSimulation(int32_t N, int32_t L);

			void Compute();

			void ComputeSpectrum();

			Texture::Texture2D displacementMap;
			Texture::Texture2D normalMap;

			Texture::Texture2D twiddleIndices;

			int32_t L;

			float waveAmplitude = 1.0f;

			vec2 windDirection = vec2(0.8f, 0.6f);
			float windSpeed = 60.0f;
			float windDependency = 0.07f;

		private:
			void ComputeTwiddleIndices();

			int32_t ReverseBits(int32_t data, int32_t bitCount);

			int32_t N;

			Shader::Shader h0;
			Shader::Shader ht;
			Shader::Shader twiddle;
			Shader::Shader horizontalButterfly;
			Shader::Shader verticalButterfly;
			Shader::Shader choppyHorizontalButterfly;
			Shader::Shader choppyVerticalButterfly;
			Shader::Shader inversion;
			Shader::Shader normal;

			// Precomputed noise textures
			Texture::Texture2D noise0;
			Texture::Texture2D noise1;
			Texture::Texture2D noise2;
			Texture::Texture2D noise3;

			Texture::Texture2D h0K;

			Texture::Texture2D hTDy;
			Texture::Texture2D hTDxz;

			Texture::Texture2D hTDyPingpong;
			Texture::Texture2D hTDxzPingpong;

			Shader::Uniform* htNUniform;
			Shader::Uniform* htLUniform;
			Shader::Uniform* htTimeUniform;

			Shader::Uniform* butterflyStageUniform;
			Shader::Uniform* butterflyPingpongUniform;
			Shader::Uniform* butterflyNUniform;
			Shader::Uniform* butterflyPreTwiddleUniform;

			Shader::Uniform* inversionNUniform;
			Shader::Uniform* inversionPingpongUniform;

			Shader::Uniform* normalNUniform;
			Shader::Uniform* normalLUniform;

		};

	}

}


#endif