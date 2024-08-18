#pragma once

#include "../System.h"
#include "../texture/Texture2D.h"
#include "../texture/Texture2DArray.h"
#include "../pipeline/PipelineConfig.h"
#include "../graphics/CommandList.h"

#include <vector>

namespace Atlas {

    namespace Ocean {

        class OceanSimulation {

        public:
            OceanSimulation() = default;

            OceanSimulation(int32_t N, int32_t L, int32_t C = 4);

            void Update(float deltaTime);

            void UpdateSpectrum();

            void Compute(Graphics::CommandList* commandList);

            Texture::Texture2DArray displacementMap;
            Texture::Texture2DArray normalMap;
            Texture::Texture2D perlinNoiseMap;

            Texture::Texture2D twiddleIndices;

            Texture::Texture2D displacementMapPrev;

            int32_t N;
            int32_t L;
            int32_t C;

            float choppinessScale = 3.0f;
            float displacementScale = 4.0f;
            float tiling = 20.0f;

            vec4 spectrumTilingFactors = vec4(1.0f, 3.0f, 6.0f, 9.0f);
            vec4 spectrumWeights = vec4(1.0f, 0.5f, 0.25f, 0.125f);
            vec4 spectrumFadeoutDistances = vec4(200.0f, 400.0f, 1000.0f, 4000.0f);

            float waveAmplitude = 0.45e-3f;
            float waveSurpression = 0.001f;

            vec2 windDirection = vec2(0.8f, 0.6f);
            float windSpeed = 100.0f;
            float windDependency = 0.9f;

            float foamTemporalWeight = 0.985f;
            float foamTemporalThreshold = 0.6f;

            float foamOffset = 0.0f;
            float foamScale = 1.5f;
            float foamSize = 4.0f;

            float simulationSpeed = 1.0f;

            bool update = true;

        private:
            void ComputeTwiddleIndices(Graphics::CommandList* commandList);

            void ComputeSpectrum(Graphics::CommandList* commandList);

            int32_t ReverseBits(int32_t data, int32_t bitCount);

            void CalculatePerlinNoiseTexture();

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

            Texture::Texture2DArray h0K;

            Texture::Texture2DArray hTD;
            Texture::Texture2DArray hTDPingpong;

        };

    }

}