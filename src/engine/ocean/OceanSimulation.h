#ifndef AE_OCEANSIMULATION_H
#define AE_OCEANSIMULATION_H

#include "../System.h"
#include "../common/NoiseGenerator.h"
#include "../texture/Texture2D.h"
#include "../pipeline/PipelineConfig.h"
#include "../graphics/CommandList.h"

#include <vector>

namespace Atlas {

	namespace Ocean {

		class OceanSimulation {

		public:
			OceanSimulation() = default;

			OceanSimulation(int32_t N, int32_t L);

            void Update(float deltaTime);

            void UpdateSpectrum();

			void Compute(Graphics::CommandList* commandList);

			Texture::Texture2D displacementMap;
			Texture::Texture2D normalMap;

			Texture::Texture2D twiddleIndices;

			Texture::Texture2D displacementMapPrev;

			int32_t L;

			float choppinessScale = 3.0f;
			float displacementScale = 4.0f;
			float tiling = 64.0f;

			float waveAmplitude = 1.0f;
			float waveSurpression = 0.001f;

			vec2 windDirection = vec2(0.8f, 0.6f);
			float windSpeed = 60.0f;
			float windDependency = 0.9f;

			float foamTemporalWeight = 0.985f;
			float foamTemporalThreshold = 0.6f;

			float foamOffset = 0.85f;
			float foamScale = 1.5f;
			float foamSize = 4.0f;

			float simulationSpeed = 1.0f;

			bool update = true;

		private:
			void ComputeTwiddleIndices(Graphics::CommandList* commandList);

            void ComputeSpectrum(Graphics::CommandList* commandList);

			int32_t ReverseBits(int32_t data, int32_t bitCount);

			int32_t N;

			float time = 0.0f;

            bool updateSpectrum = true;
            bool updateTwiddleIndices = true;

            PipelineConfig h0Config;
            PipelineConfig htConfig;
            PipelineConfig twiddleConfig;
            PipelineConfig horizontalButterflyConfig;
            PipelineConfig verticalButterflyConfig;
            PipelineConfig inversionConfig;
            PipelineConfig normalConfig;

			// Precomputed noise textures
			Texture::Texture2D noise0;
			Texture::Texture2D noise1;
			Texture::Texture2D noise2;
			Texture::Texture2D noise3;

			Texture::Texture2D h0K;

			Texture::Texture2D hTD;
			Texture::Texture2D hTDPingpong;

            /*
			OldShader::Uniform* htNUniform;
			OldShader::Uniform* htLUniform;
			OldShader::Uniform* htTimeUniform;

			OldShader::Uniform* butterflyStageUniform;
			OldShader::Uniform* butterflyPingpongUniform;
			OldShader::Uniform* butterflyNUniform;
			OldShader::Uniform* butterflyPreTwiddleUniform;

			OldShader::Uniform* inversionNUniform;
			OldShader::Uniform* inversionPingpongUniform;

			OldShader::Uniform* normalNUniform;
			OldShader::Uniform* normalLUniform;
			OldShader::Uniform* normalChoppyScaleUniform;
			OldShader::Uniform* normalDisplacementScaleUniform;
			OldShader::Uniform* normalTilingUniform;
			OldShader::Uniform* normalFoamTemporalWeightUniform;
			OldShader::Uniform* normalFoamTemporalThresholdUniform;
			OldShader::Uniform* normalFoamOffsetUniform;
            */

		};

	}

}


#endif