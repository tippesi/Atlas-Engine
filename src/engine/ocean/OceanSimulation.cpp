#include "OceanSimulation.h"
#include "../Clock.h"
#include "../buffer/Buffer.h"
#include "../graphics/Profiler.h"
#include "../pipeline/PipelineManager.h"
#include "../common/NoiseGenerator.h"

#include <vector>

bool set = false;

namespace Atlas {

    namespace Ocean {

        OceanSimulation::OceanSimulation(int32_t N, int32_t L, int32_t C) : N(N), L(L), C(C) {

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

            twiddleIndices = Texture::Texture2D((int32_t)log2((float)N), N, VK_FORMAT_R32G32_SFLOAT);
            hTD = Texture::Texture2DArray(N, N, C, VK_FORMAT_R32G32B32A32_SFLOAT);
            hTDPingpong = Texture::Texture2DArray(N, N, C, VK_FORMAT_R32G32B32A32_SFLOAT);

            h0Config = PipelineConfig("ocean/h0.csh");
            htConfig = PipelineConfig("ocean/ht.csh");
            twiddleConfig = PipelineConfig("ocean/twiddleIndices.csh");
            horizontalButterflyConfig = PipelineConfig("ocean/butterfly.csh");
            verticalButterflyConfig = PipelineConfig("ocean/butterfly.csh");
            inversionConfig = PipelineConfig("ocean/inversion.csh");
            normalConfig = PipelineConfig("ocean/normal.csh");

            horizontalButterflyConfig.AddMacro("HORIZONTAL");
            verticalButterflyConfig.AddMacro("VERTICAL");

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
                ivec4 L;
                vec2 w = vec2(1.0f);
                int N = 1;
                float A = 1.0f;
                float windSpeed = 1.0f;
                float windDependency = 1.0f;
                float waveSurpression = 1.0f;
            };

            Graphics::Profiler::BeginQuery("Compute ocean spectrum");

            auto pipeline = PipelineManager::GetPipeline(h0Config);
            commandList->BindPipeline(pipeline);

            PushConstants constants{
                .L = ivec4(L, L * 2, L * 4, L * 8),
                .w = windDirection,
                .N = N,
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

        void OceanSimulation::ComputeTwiddleIndices(Graphics::CommandList* commandList) {

            auto bitCount = (int32_t)log2f((float)N);
            std::vector<int32_t> indices(N);

            for (int32_t i = 0; i < N; i++) {
                indices[i] = ReverseBits(i, bitCount);
            }

            auto usage = Buffer::BufferUsageBits::StorageBufferBit | Buffer::BufferUsageBits::HostAccessBit;
            Buffer::Buffer buffer(usage, sizeof(int32_t), indices.size(), indices.data());

            auto pipeline = PipelineManager::GetPipeline(twiddleConfig);
            commandList->BindPipeline(pipeline);

            commandList->ImageMemoryBarrier(twiddleIndices.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);

            commandList->BindImage(twiddleIndices.image, 3, 0);
            buffer.Bind(commandList, 3, 1);

            commandList->Dispatch(bitCount, N / 16, 1);

            commandList->ImageMemoryBarrier(twiddleIndices.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT);

        }

        void OceanSimulation::Compute(Graphics::CommandList* commandList) {

            if (updateSpectrum) {
                ComputeSpectrum(commandList);
                updateSpectrum = false;
            }

            if (updateTwiddleIndices) {
                ComputeTwiddleIndices(commandList);
                updateTwiddleIndices = false;
            }

            Graphics::Profiler::BeginQuery("Compute ocean simulation");

            

            // displacementMapPrev.Copy(displacementMap);

            {
                struct alignas(16) PushConstants {
                    ivec4 L;
                    int N = 1;
                    float time = 1.0f;
                };

                Graphics::Profiler::BeginQuery("Compute h(t)");

                auto pipeline = PipelineManager::GetPipeline(htConfig);
                commandList->BindPipeline(pipeline);

                PushConstants constants = {
                    .L = ivec4(L, L * 2, L * 4, L * 8),
                    .N = N,
                    .time = simulationSpeed * time
                };
                commandList->PushConstants("constants", &constants);

                Graphics::ImageBarrier imageBarriers[] = {
                    {hTD.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                    {h0K.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT}
                };
                commandList->PipelineBarrier(imageBarriers, {});

                commandList->BindImage(hTD.image, 3, 0);
                commandList->BindImage(h0K.image, 3, 1);

                commandList->Dispatch(N / 16, N / 16, C);

                Graphics::Profiler::EndQuery();
            }

            int32_t pingpong = 0;
            int32_t log2n = (int32_t)log2f((float)N);

            Graphics::Profiler::BeginQuery("Perform iFFT");

            commandList->BindImage(twiddleIndices.image, 3, 0);

            for (uint8_t j = 0; j < 2; j++) {

                struct alignas(16) PushConstants {
                    int stage = 1;
                    int pingpong = 1;
                    int N = 1;
                    float preTwiddle = 1.0f;
                };

                // The first two passes calculate the height,
                // while the second two passes calculate choppyness.
                Ref<Graphics::Pipeline> pipeline;
                switch (j) {
                    case 0: pipeline = PipelineManager::GetPipeline(horizontalButterflyConfig); break;
                    case 1: pipeline = PipelineManager::GetPipeline(verticalButterflyConfig); break;
                    default: break;
                }

                commandList->BindPipeline(pipeline);

                for (int32_t i = 0; i < log2n; i++) {

                    if (!pingpong) {
                        Graphics::ImageBarrier imageBarriers[] = {
                            {hTD.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT},
                            {hTDPingpong.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT}
                        };
                        commandList->BindImage(hTD.image, 3, 1);
                        commandList->BindImage(hTDPingpong.image, 3, 2);
                        commandList->PipelineBarrier(imageBarriers, {});
                    }
                    else {
                        Graphics::ImageBarrier imageBarriers[] = {
                            {hTD.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                            {hTDPingpong.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT}
                        };
                        commandList->BindImage(hTDPingpong.image, 3, 1);
                        commandList->BindImage(hTD.image, 3, 2);
                        commandList->PipelineBarrier(imageBarriers, {});
                    }                   

                    auto preTwiddle = (float)N / powf(2.0f, (float)i + 1.0f);

                    PushConstants constants = {
                        .stage = i,
                        .pingpong = pingpong,
                        .N = N,
                        .preTwiddle = preTwiddle
                    };
                    commandList->PushConstants("constants", &constants);

                    commandList->Dispatch(N / 8, N / 8, C);

                    pingpong = (pingpong + 1) % 2;
                }
            }

            Graphics::Profiler::EndQuery();

            {
                Graphics::Profiler::BeginQuery("Compute heightfield/displacement");

                auto pipeline = PipelineManager::GetPipeline(inversionConfig);
                commandList->BindPipeline(pipeline);

                Ref<Graphics::Image> image;
                if (pingpong == 0) {
                    image = hTD.image;
                }
                else {
                    image = hTDPingpong.image;
                }

                Graphics::ImageBarrier imageBarriers[] = {
                    {displacementMap.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                    {image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT},
                };
                commandList->PipelineBarrier(imageBarriers, {});

                commandList->BindImage(displacementMap.image, 3, 0);
                commandList->BindImage(image, 3, 1);

                commandList->Dispatch(N / 16, N / 16, C);

                Graphics::Profiler::EndQuery();
            }

            {
                struct alignas(16) PushConstants {
                    ivec4 L;
                    vec4 tilingFactors;
                    vec4 spectrumScaling;
                    int N = 1;
                    float choppyScale = 1.0f;
                    float displacementScale = 1.0f;
                    float tiling = 1.0f;

                    float temporalWeight = 1.0f;
                    float temporalThreshold = 1.0f;

                    float foamOffset = 1.0f;
                };

                Graphics::Profiler::BeginQuery("Compute normal map");

                auto pipeline = PipelineManager::GetPipeline(normalConfig);
                commandList->BindPipeline(pipeline);

                PushConstants constants = {
                    .L = ivec4(L, L * 2, L * 4, L * 8),
                    .tilingFactors = spectrumTilingFactors,
                    .spectrumScaling = spectrumWeights,
                    .N = N,
                    .choppyScale = choppinessScale,
                    .displacementScale = displacementScale,
                    .tiling = tiling,

                    .temporalWeight = foamTemporalWeight,
                    .temporalThreshold = foamTemporalThreshold,

                    .foamOffset = foamOffset
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

        int32_t OceanSimulation::ReverseBits(int32_t data, int32_t bitCount) {

            int32_t reversed = 0;

            for (int32_t i = 0; i < bitCount; i++) {
                if (data & (1 << i))
                    reversed |= (1 << ((bitCount - 1) - i));
            }

            return reversed;

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