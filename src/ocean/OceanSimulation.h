#ifndef AE_OCEANSIMULATION_H
#define AE_OCEANSIMULATION_H

#include "../System.h"
#include "../common/NoiseGenerator.h"
#include "../texture/Texture2D.h"
#include "../shader/Shader.h"

#include <vector>

namespace Atlas {

	namespace Ocean {

		class OceanSimulation {

		public:
			OceanSimulation() {}

			OceanSimulation(int32_t N, int32_t L);

			void Compute(float deltaTime);

			void ComputeSpectrum();

			Texture::Texture2D displacementMap;
			Texture::Texture2D normalMap;

			Texture::Texture2D twiddleIndices;

			Texture::Texture2D displacementMapPrev;

			int32_t L;

			float choppinessScale = 3.0f;
			float displacementScale = 4.0f;
			float tiling = 64.0f;

			float waveAmplitude = 1.0f;

			vec2 windDirection = vec2(0.8f, 0.6f);
			float windSpeed = 60.0f;
			float windDependency = 0.9f;

			float foamTemporalWeight = 0.985f;
			float foamTemporalThreshold = 0.6f;

			float foamOffset = 0.85f;
			float foamScale = 1.5f;
			float foamSize = 4.0f;

			float simulationSpeed = 5.0f;

		private:
			void ComputeTwiddleIndices();

			int32_t ReverseBits(int32_t data, int32_t bitCount);

			int32_t N;

			float time = 0.0f;

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
			Shader::Uniform* normalChoppyScaleUniform;
			Shader::Uniform* normalDisplacementScaleUniform;
			Shader::Uniform* normalTilingUniform;
			Shader::Uniform* normalFoamTemporalWeightUniform;
			Shader::Uniform* normalFoamTemporalThresholdUniform;
			Shader::Uniform* normalFoamOffsetUniform;

		};

	}

}


#endif