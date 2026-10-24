#include "OceanSimulation.h"
#include "../Clock.h"
#include "../graphics/Profiler.h"
#include "../pipeline/PipelineManager.h"
#include "../common/NoiseGenerator.h"

#include <vector>

bool set = false;

namespace Atlas {

    namespace Ocean {

        OceanSimulation::OceanSimulation(int32_t N, int32_t C) : N(N), C(C) {

            noise0 = Texture::Texture2D(N, N, VK_FORMAT_R8_UNORM);
            noise1 = Texture::Texture2D(N, N, VK_FORMAT_R8_UNORM);
            noise2 = Texture::Texture2D(N, N, VK_FORMAT_R8_UNORM);
            noise3 = Texture::Texture2D(N, N, VK_FORMAT_R8_UNORM);

            Common::Image<uint8_t> image0(N, N, 1);
            Common::Image<uint8_t> image1(N, N, 1);
            Common::Image<uint8_t> image2(N, N, 1);
            Common::Image<uint8_t> image3(N, N, 1);

            Common::NoiseGenerator::GenerateUniformNoise2D(image0);
            Common::NoiseGenerator::GenerateUniformNoise2D(image1);
            Common::NoiseGenerator::GenerateUniformNoise2D(image2);
            Common::NoiseGenerator::GenerateUniformNoise2D(image3);

            noise0.SetData(image0.GetData());
            noise1.SetData(image1.GetData());
            noise2.SetData(image2.GetData());
            noise3.SetData(image3.GetData());

            displacementMap = Texture::Texture2DArray(N, N, C, VK_FORMAT_R16G16B16A16_SFLOAT,
                Texture::Wrapping::Repeat, Texture::Filtering::Anisotropic);
            normalMap = Texture::Texture2DArray(N, N, C, VK_FORMAT_R16G16B16A16_SFLOAT,
                Texture::Wrapping::Repeat, Texture::Filtering::Anisotropic);
            perlinNoiseMap = Texture::Texture2D(N * 2, N * 2, VK_FORMAT_R32G32B32A32_SFLOAT,
                Texture::Wrapping::Repeat, Texture::Filtering::Linear);
            displacementMapPrev = Texture::Texture2D(N, N, VK_FORMAT_R16G16B16A16_SFLOAT,
                Texture::Wrapping::Repeat, Texture::Filtering::Linear);

            h0K = Texture::Texture2DArray(N, N, C, VK_FORMAT_R32G32_SFLOAT);

            hTD = Texture::Texture2DArray(N, N, C, VK_FORMAT_R32G32B32A32_SFLOAT);

            h0Config = PipelineConfig("ocean/h0.csh");
            horizontalFFTConfig = PipelineConfig("ocean/fft.csh");
            verticalFFTConfig = PipelineConfig("ocean/fft.csh");
            normalConfig = PipelineConfig("ocean/normal.csh");

            // Each work group transforms a whole row/column in shared memory
            horizontalFFTConfig.AddMacro("HORIZONTAL");
            horizontalFFTConfig.AddMacro("FFT_SIZE " + std::to_string(N));
            verticalFFTConfig.AddMacro("VERTICAL");
            verticalFFTConfig.AddMacro("FFT_SIZE " + std::to_string(N));

            CalculatePerlinNoiseTexture();

        }

        void OceanSimulation::Update(float deltaTime) {

            if (!update) return;

            time += deltaTime;

        }

        void OceanSimulation::UpdateSpectrum() {

            updateSpectrum = true;

        }

        void OceanSimulation::ComputeSpectrum(Graphics::CommandList* commandList) {

            struct alignas(16) PushConstants {
                vec4 L;
                vec2 w = vec2(1.0f);
                int N = 1;
                int C = 1;
                float A = 1.0f;
                float windSpeed = 1.0f;
                float windDependency = 1.0f;
                float waveSurpression = 1.0f;
            };

            Graphics::Profiler::BeginQuery("Compute ocean spectrum");

            auto pipeline = PipelineManager::GetPipeline(h0Config);
            commandList->BindPipeline(pipeline);

            PushConstants constants{
                .L = spectrumPatchSizes,
                .w = windDirection,
                .N = N,
                .C = C,
                .A = waveAmplitude,
                .windSpeed = windSpeed,
                .windDependency = windDependency,
                .waveSurpression = waveSurpression,
            };
            commandList->PushConstants("constants", &constants);

            noise0.Bind(commandList, 3, 2);
            noise1.Bind(commandList, 3, 3);
            noise2.Bind(commandList, 3, 4);
            noise3.Bind(commandList, 3, 5);

            // Need to write here, so don't bind as combined image sampler
            commandList->BindImage(h0K.image, 3, 0);

            commandList->ImageMemoryBarrier(h0K.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);

            commandList->Dispatch(N / 16, N / 16, C);

            commandList->ImageMemoryBarrier(h0K.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);

            Graphics::Profiler::EndQuery();

        }

        void OceanSimulation::Compute(Graphics::CommandList* commandList) {

            // The spectrum of each cascade is generated for the size its tile has in the world
            auto patchSizes = tiling * spectrumTilingFactors;
            if (patchSizes != spectrumPatchSizes) {
                spectrumPatchSizes = patchSizes;
                updateSpectrum = true;
            }

            if (updateSpectrum) {
                ComputeSpectrum(commandList);
                updateSpectrum = false;
            }

            Graphics::Profiler::BeginQuery("Compute ocean simulation");

            // displacementMapPrev.Copy(displacementMap);

            struct alignas(16) PushConstants {
                vec4 L;
                vec4 spectrumScaling;
                float time = 1.0f;
                float choppyScale = 1.0f;
                float displacementScale = 1.0f;
            };

            PushConstants constants = {
                .L = spectrumPatchSizes,
                .spectrumScaling = spectrumWeights,
                .time = simulationSpeed * time,
                .choppyScale = choppinessScale,
                .displacementScale = displacementScale
            };

            // The normal map can be calculated by the FFT directly instead of finite differences
            horizontalFFTConfig.ManageMacro("NORMALS", fftNormals);
            verticalFFTConfig.ManageMacro("NORMALS", fftNormals);

            if (fftNormals && !hTDSlopes.IsValid())
                hTDSlopes = Texture::Texture2DArray(N, N, C, VK_FORMAT_R32G32B32A32_SFLOAT);
            if (!fftNormals && hTDSlopes.IsValid())
                hTDSlopes = Texture::Texture2DArray();

            {
                Graphics::Profiler::BeginQuery("Compute h(t) and horizontal iFFT");

                auto pipeline = PipelineManager::GetPipeline(horizontalFFTConfig);
                commandList->BindPipeline(pipeline);

                commandList->PushConstants("constants", &constants);

                Graphics::ImageBarrier imageBarriers[] = {
                    {hTD.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                    {h0K.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT}
                };
                commandList->PipelineBarrier(imageBarriers, {});

                commandList->BindImage(hTD.image, 3, 0);
                commandList->BindImage(h0K.image, 3, 1);
                if (fftNormals) {
                    commandList->ImageMemoryBarrier(hTDSlopes.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);
                    commandList->BindImage(hTDSlopes.image, 3, 2);
                }

                commandList->Dispatch(1, N, C);

                Graphics::Profiler::EndQuery();
            }

            {
                Graphics::Profiler::BeginQuery("Compute vertical iFFT and displacement");

                auto pipeline = PipelineManager::GetPipeline(verticalFFTConfig);
                commandList->BindPipeline(pipeline);

                commandList->PushConstants("constants", &constants);

                Graphics::ImageBarrier imageBarriers[] = {
                    {displacementMap.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                    {hTD.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT},
                };
                commandList->PipelineBarrier(imageBarriers, {});

                commandList->BindImage(displacementMap.image, 3, 0);
                commandList->BindImage(hTD.image, 3, 1);
                if (fftNormals) {
                    Graphics::ImageBarrier normalBarriers[] = {
                        {normalMap.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                        {hTDSlopes.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT},
                    };
                    commandList->PipelineBarrier(normalBarriers, {});

                    commandList->BindImage(normalMap.image, 3, 2);
                    commandList->BindImage(hTDSlopes.image, 3, 3);
                }

                commandList->Dispatch(N, 1, C);

                Graphics::Profiler::EndQuery();
            }

            if (!fftNormals) {
                struct alignas(16) PushConstants {
                    vec4 tilingFactors;
                    vec4 spectrumScaling;
                    int N = 1;
                    float choppyScale = 1.0f;
                    float displacementScale = 1.0f;
                    float tiling = 1.0f;

                    float temporalWeight = 1.0f;
                    float temporalThreshold = 1.0f;
                };

                Graphics::Profiler::BeginQuery("Compute normal map");

                auto pipeline = PipelineManager::GetPipeline(normalConfig);
                commandList->BindPipeline(pipeline);

                PushConstants constants = {
                    .tilingFactors = spectrumTilingFactors,
                    .spectrumScaling = spectrumWeights,
                    .N = N,
                    .choppyScale = choppinessScale,
                    .displacementScale = displacementScale,
                    .tiling = tiling,

                    .temporalWeight = foamTemporalWeight,
                    .temporalThreshold = foamTemporalThreshold
                };
                commandList->PushConstants("constants", &constants);

                Graphics::ImageBarrier imageBarriers[] = {
                    {displacementMap.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT},
                    {normalMap.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                };
                commandList->PipelineBarrier(imageBarriers, {});

                commandList->BindImage(displacementMap.image, 3, 0);
                commandList->BindImage(normalMap.image, 3, 1);

                commandList->Dispatch(N / 16, N / 16, C);

                Graphics::Profiler::EndQuery();
            }

            Graphics::Profiler::BeginQuery("Build normal map mips");

            commandList->ImageMemoryBarrier(normalMap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

            // The cube map generating automatically transforms the image layout to read-only optimal
            commandList->GenerateMipMaps(normalMap.image);

            commandList->ImageMemoryBarrier(displacementMap.image,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            commandList->ImageMemoryBarrier(displacementMap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

            // The cube map generating automatically transforms the image layout to read-only optimal
            commandList->GenerateMipMaps(displacementMap.image);

            commandList->ImageMemoryBarrier(displacementMap.image,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            Graphics::Profiler::EndQuery();
            Graphics::Profiler::EndQuery();

        }

        void OceanSimulation::CalculatePerlinNoiseTexture() {



            Common::Image<float> image(perlinNoiseMap.width, perlinNoiseMap.height, 1);
            std::vector<float> amplitudes = {1.0f, 1.0f, 1.0f, 1.f, 1.f, 1.f, 0.125f ,
                                             0.125f / 2.0f, 0.125f / 2.0f, 0.125f / 4.0f};
            Common::NoiseGenerator::GeneratePerlinNoise2D(image, amplitudes, 0);

            Common::Image<float> noiseImage(perlinNoiseMap.width, perlinNoiseMap.height, 4);
            for (int32_t y = 0; y < image.height; y++) {
                for (int32_t x = 0; x < image.height; x++) {
                    float center = image.Sample(x, y).r;
                    float left = image.Sample(x - 1, y).r;
                    float right = image.Sample(x + 1, y).r;
                    float top = image.Sample(x, y - 1).r;
                    float bottom = image.Sample(x, y + 1).r;

                    vec2 gradient = vec2(left - right, bottom - top);
                    noiseImage.SetData(x, y, 0, center);
                    noiseImage.SetData(x, y, 1, gradient.x);
                    noiseImage.SetData(x, y, 2, gradient.y);
                }
            }

            perlinNoiseMap.SetData(noiseImage.GetData());

        }

    }

}