#include "OceanSimulation.h"
#include "../Clock.h"
#include "../buffer/UniformBuffer.h"
#include "../graphics/Profiler.h"
#include "../pipeline/PipelineManager.h"

#include <vector>

bool set = false;

namespace Atlas {

	namespace Ocean {

		OceanSimulation::OceanSimulation(int32_t N, int32_t L) : N(N), L(L) {

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

			displacementMap = Texture::Texture2D(N, N, VK_FORMAT_R16G16B16A16_SFLOAT,
                Texture::Wrapping::Repeat, Texture::Filtering::Linear);
			normalMap = Texture::Texture2D(N, N, VK_FORMAT_R16G16B16A16_SFLOAT,
                Texture::Wrapping::Repeat, Texture::Filtering::Linear);
			displacementMapPrev = Texture::Texture2D(N, N, VK_FORMAT_R16G16B16A16_SFLOAT,
                Texture::Wrapping::Repeat, Texture::Filtering::Linear);

			h0K = Texture::Texture2D(N, N, VK_FORMAT_R32G32_SFLOAT);

			twiddleIndices = Texture::Texture2D((int32_t)log2((float)N), N, VK_FORMAT_R32G32_SFLOAT);
			hTD = Texture::Texture2D(N, N, VK_FORMAT_R32G32B32A32_SFLOAT);
			hTDPingpong = Texture::Texture2D(N, N, VK_FORMAT_R32G32B32A32_SFLOAT);

            h0Config = PipelineConfig("ocean/h0.csh");
            htConfig = PipelineConfig("ocean/ht.csh");
            twiddleConfig = PipelineConfig("ocean/twiddleIndices.csh");
            horizontalButterflyConfig = PipelineConfig("ocean/butterfly.csh");
            verticalButterflyConfig = PipelineConfig("ocean/butterfly.csh");
            inversionConfig = PipelineConfig("ocean/inversion.csh");
            normalConfig = PipelineConfig("ocean/normal.csh");

			horizontalButterflyConfig.AddMacro("HORIZONTAL");
			verticalButterflyConfig.AddMacro("VERTICAL");

		}

        void OceanSimulation::Update(float deltaTime) {

            time += deltaTime;

        }

        void OceanSimulation::UpdateSpectrum() {

            updateSpectrum = true;

        }

		void OceanSimulation::ComputeSpectrum(Graphics::CommandList* commandList) {

            struct alignas(16) PushConstants {
                int N;
                int L;
                float A;
                float windSpeed;
                float windDependency;
                float waveSurpression;
                vec2 w;
            };

			Graphics::Profiler::BeginQuery("Compute ocean spectrum");

            auto pipeline = PipelineManager::GetPipeline(h0Config);
            commandList->BindPipeline(pipeline);

            PushConstants constants {
                .N = N,
                .L = L,
                .A = waveAmplitude,
                .windSpeed = windSpeed,
                .windDependency = windDependency,
                .waveSurpression = waveSurpression,
                .w = windDirection
            };
            commandList->PushConstants("constants", &constants);

			noise0.Bind(commandList, 3, 2);
			noise1.Bind(commandList, 3, 3);
			noise2.Bind(commandList, 3, 4);
			noise3.Bind(commandList, 3, 5);

            // Need to write here, so don't bind as combined image sampler
            commandList->BindImage(h0K.image, 3, 0);

			commandList->ImageMemoryBarrier(h0K.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);

            commandList->Dispatch(N / 16, N / 16, 1);

            commandList->ImageMemoryBarrier(h0K.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);

			Graphics::Profiler::EndQuery();

		}

		void OceanSimulation::ComputeTwiddleIndices(Graphics::CommandList* commandList) {

			auto bitCount = (int32_t)log2f((float)N);
			std::vector<int32_t> indices(N);

			for (int32_t i = 0; i < N; i++) {
				indices[i] = ReverseBits(i, bitCount);
			}

			Buffer::UniformBuffer buffer(sizeof(int32_t), indices.size());
			buffer.SetData(indices.data(), 0, indices.size());

            commandList->ImageMemoryBarrier(twiddleIndices.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);

            commandList->BindImage(twiddleIndices.image, 3, 0);
            buffer.Bind(commandList, 3, 1);

			commandList->Dispatch(bitCount, N / 16, 1);

            commandList->ImageMemoryBarrier(twiddleIndices.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);

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

			/*
            Graphics::Profiler::BeginQuery("Compute ocean simulation");

			time += deltaTime;

			if (!update) return;

			// displacementMapPrev.Copy(displacementMap);

            Graphics::Profiler::BeginQuery("Compute h(t)");

			ht.Bind();

			htNUniform->SetValue(N);
			htLUniform->SetValue(L);
			htTimeUniform->SetValue(simulationSpeed * time);

			hTD.Bind(GL_WRITE_ONLY, 0);
			h0K.Bind(GL_READ_ONLY, 1);

			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
				GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			glDispatchCompute(N / 16, N / 16, 1);


			Profiler::EndQuery();

			int32_t pingpong = 0;
			int32_t log2n = (int32_t)log2f((float)N);

			Profiler::BeginQuery("Perform iFFT");

            // twiddleIndices.Bind(GL_READ_ONLY, 0);

			for (uint8_t j = 0; j < 2; j++) {

				// The first two passes calculate the height,
				// while the second two passes calculate choppyness.
				switch (j) {
				case 0: horizontalButterfly.Bind();  break;
				case 1: verticalButterfly.Bind(); break;
				default: break;
				}

				butterflyNUniform->SetValue(N);

				for (int32_t i = 0; i < log2n; i++) {

					if (!pingpong) {
						hTD.Bind(GL_READ_ONLY, 1);
						hTDPingpong.Bind(GL_WRITE_ONLY, 2);
					}
					else {
						hTDPingpong.Bind(GL_READ_ONLY, 1);
						hTD.Bind(GL_WRITE_ONLY, 2);
					}

					auto preTwiddle = (float)N / powf(2.0f, (float)i + 1.0f);

					butterflyPreTwiddleUniform->SetValue(preTwiddle);

					butterflyStageUniform->SetValue(i);
					butterflyPingpongUniform->SetValue(pingpong);

					glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
					glDispatchCompute(N / 8, N / 8, 1);

					pingpong = (pingpong + 1) % 2;
				}
			}

			Profiler::EndQuery();

			Profiler::BeginQuery("Compute heightfield/displacement");

			// Inverse and correct the texture
			inversion.Bind();

			inversionNUniform->SetValue(N);
			inversionPingpongUniform->SetValue(pingpong);

			displacementMap.Bind(GL_WRITE_ONLY, 0);

            if (pingpong == 0) {
                hTD.Bind(GL_READ_ONLY, 1);
            }
            else {
                hTDPingpong.Bind(GL_READ_ONLY, 1);
            }

			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			glDispatchCompute(N / 16, N / 16, 1);

			Profiler::EndQuery();

			Profiler::BeginQuery("Compute normal map");

			// Calculate normals
			normal.Bind();

			normalNUniform->SetValue(N);
			normalLUniform->SetValue(L);
			normalChoppyScaleUniform->SetValue(choppinessScale);
			normalDisplacementScaleUniform->SetValue(displacementScale);
			normalTilingUniform->SetValue(tiling);
			normalFoamTemporalWeightUniform->SetValue(foamTemporalWeight);
			normalFoamTemporalThresholdUniform->SetValue(foamTemporalThreshold);
			normalFoamOffsetUniform->SetValue(foamOffset);

			auto normalMapCopy(normalMap);

			displacementMap.Bind(GL_READ_ONLY, 0);
			normalMap.Bind(GL_WRITE_ONLY, 1);
			normalMapCopy.Bind(GL_READ_ONLY, 2);

			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			glDispatchCompute(N / 16, N / 16, 1);

			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			Profiler::BeginQuery("Build normal map mips");
			normalMap.Bind();
			normalMap.GenerateMipmap();
			Profiler::EndQuery();
			Profiler::EndQuery();
			Profiler::EndQuery();
             */

		}

		int32_t OceanSimulation::ReverseBits(int32_t data, int32_t bitCount) {

			int32_t reversed = 0;

			for (int32_t i = 0; i < bitCount; i++) {
				if (data & (1 << i))
					reversed |= (1 << ((bitCount - 1) - i));
			}

			return reversed;

		}

	}

}