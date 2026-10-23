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

            OceanSimulation(int32_t N, int32_t C = 4);

            void Update(float deltaTime);

            void UpdateSpectrum();

            void Compute(Graphics::CommandList* commandList);

            Texture::Texture2DArray displacementMap;
            Texture::Texture2DArray normalMap;
            Texture::Texture2D perlinNoiseMap;

            Texture::Texture2D displacementMapPrev;

            int32_t N;
            int32_t C;

            float choppinessScale = 3.0f;
            float displacementScale = 4.0f;
            float tiling = 20.0f;

            vec4 spectrumTilingFactors = vec4(1.0f, 3.7f, 13.1f, 47.3f);
            vec4 spectrumWeights = vec4(1.0f);
            vec4 spectrumFadeoutDistances = vec4(250.0f, 800.0f, 3000.0f, 12000.0f);

            float waveAmplitude = 1.1e-3f;
            float waveSurpression = 0.001f;

            vec2 windDirection = vec2(0.8f, 0.6f);
            float windSpeed = 12.0f;
            float windDependency = 0.1f;

            float foamTemporalWeight = 0.985f;
            float foamTemporalThreshold = 0.6f;

            float foamOffset = 0.6f;
            float foamScale = 4.0f;
            float foamSize = 4.0f;

            float simulationSpeed = 1.0f;

            bool update = true;
            bool fftNormals = false;

        private:
            void ComputeSpectrum(Graphics::CommandList* commandList);

            void CalculatePerlinNoiseTexture();

            float time = 0.0f;

            bool updateSpectrum = true;
            vec4 spectrumPatchSizes = vec4(0.0f);

            PipelineConfig h0Config;
            PipelineConfig horizontalFFTConfig;
            PipelineConfig verticalFFTConfig;
            PipelineConfig normalConfig;

            // Precomputed noise textures
            Texture::Texture2D noise0;
            Texture::Texture2D noise1;
            Texture::Texture2D noise2;
            Texture::Texture2D noise3;

            Texture::Texture2DArray h0K;

            Texture::Texture2DArray hTD;
            Texture::Texture2DArray hTDSlopes;

        };

    }

}