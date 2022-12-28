#include "OceanSimulation.h"
#include "../Clock.h"
#include "../buffer/Buffer.h"
#include "../graphics/Profiler.h"

#include <vector>

bool set = false;

namespace Atlas {

	namespace Ocean {

		OceanSimulation::OceanSimulation(int32_t N, int32_t L) : N(N), L(L) {

            /*
			noise0 = Texture::Texture2D(N, N, AE_R8);
			noise1 = Texture::Texture2D(N, N, AE_R8);
			noise2 = Texture::Texture2D(N, N, AE_R8);
			noise3 = Texture::Texture2D(N, N, AE_R8);
            */

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

            /*
			displacementMap = Texture::Texture2D(N, N, AE_RGBA16F, GL_REPEAT, GL_LINEAR, true, true);
			normalMap = Texture::Texture2D(N, N, AE_RGBA16F, GL_REPEAT, GL_LINEAR, true, true);

			displacementMapPrev = Texture::Texture2D(N, N, AE_RGBA16F, GL_REPEAT, GL_LINEAR, true, true);

			h0K = Texture::Texture2D(N, N, AE_RG32F);

			twiddleIndices = Texture::Texture2D((int32_t)log2((float)N), N, AE_RG32F);
			hTD = Texture::Texture2D(N, N, AE_RGBA32F);
			hTDPingpong = Texture::Texture2D(N, N, AE_RGBA32F);

			h0.AddStage(AE_COMPUTE_STAGE, "ocean/h0.csh");
			ht.AddStage(AE_COMPUTE_STAGE, "ocean/ht.csh");
			twiddle.AddStage(AE_COMPUTE_STAGE, "ocean/twiddleIndices.csh");
			horizontalButterfly.AddStage(AE_COMPUTE_STAGE, "ocean/butterfly.csh");
			verticalButterfly.AddStage(AE_COMPUTE_STAGE, "ocean/butterfly.csh");
			inversion.AddStage(AE_COMPUTE_STAGE, "ocean/inversion.csh");
			normal.AddStage(AE_COMPUTE_STAGE, "ocean/normal.csh");
             */

			horizontalButterfly.AddMacro("HORIZONTAL");
			verticalButterfly.AddMacro("VERTICAL");

			htNUniform = ht.GetUniform("N");
			htLUniform = ht.GetUniform("L");
			htTimeUniform = ht.GetUniform("time");

			butterflyStageUniform = horizontalButterfly.GetUniform("stage");
			butterflyPingpongUniform = horizontalButterfly.GetUniform("pingpong");
			butterflyNUniform = horizontalButterfly.GetUniform("N");
			butterflyPreTwiddleUniform = horizontalButterfly.GetUniform("preTwiddle");

			inversionNUniform = inversion.GetUniform("N");
			inversionPingpongUniform = inversion.GetUniform("pingpong");

			normalNUniform = normal.GetUniform("N");
			normalLUniform = normal.GetUniform("L");
			normalChoppyScaleUniform = normal.GetUniform("choppyScale");
			normalDisplacementScaleUniform = normal.GetUniform("displacementScale");
			normalTilingUniform = normal.GetUniform("tiling");
			normalFoamTemporalWeightUniform = normal.GetUniform("temporalWeight");
			normalFoamTemporalThresholdUniform = normal.GetUniform("temporalThreshold");
			normalFoamOffsetUniform = normal.GetUniform("foamOffset");

			ComputeTwiddleIndices();
			ComputeSpectrum();

		}

		void OceanSimulation::ComputeSpectrum() {

			Graphics::Profiler::BeginQuery("Compute ocean spectrum");

			h0.Bind();

			h0.GetUniform("N")->SetValue(N);
			h0.GetUniform("L")->SetValue(L);
			h0.GetUniform("A")->SetValue(waveAmplitude);
			h0.GetUniform("w")->SetValue(windDirection);
			h0.GetUniform("windspeed")->SetValue(windSpeed);
			h0.GetUniform("windDependency")->SetValue(windDependency);
			h0.GetUniform("waveSurpression")->SetValue(waveSurpression);

            /*
			noise0.Bind(2);
			noise1.Bind(3);
			noise2.Bind(4);
			noise3.Bind(5);

			h0K.Bind(GL_WRITE_ONLY, 0);

			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
				GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			glDispatchCompute(N / 16, N / 16, 1);
             */

			Graphics::Profiler::EndQuery();

		}

		void OceanSimulation::ComputeTwiddleIndices() {

			auto nUniform = twiddle.GetUniform("N");

			auto bitCount = (int32_t)log2f((float)N);
			std::vector<int32_t> indices(N);

			for (int32_t i = 0; i < N; i++) {
				indices[i] = ReverseBits(i, bitCount);
			}

            /*
			Buffer::Buffer buffer(AE_SHADER_STORAGE_BUFFER, sizeof(int32_t), AE_BUFFER_DYNAMIC_STORAGE);
			buffer.SetSize(indices.size());
			buffer.SetData(indices.data(), 0, indices.size());

			nUniform->SetValue(N);

			twiddleIndices.Bind(GL_WRITE_ONLY, 0);
			buffer.BindBase(1);

			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
				GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			glDispatchCompute(bitCount, N / 16, 1);
             */

		}

		void OceanSimulation::Compute(float deltaTime) {

            Graphics::Profiler::BeginQuery("Compute ocean simulation");

			time += deltaTime;

			if (!update) return;

			// displacementMapPrev.Copy(displacementMap);

            Graphics::Profiler::BeginQuery("Compute h(t)");

			ht.Bind();

			htNUniform->SetValue(N);
			htLUniform->SetValue(L);
			htTimeUniform->SetValue(simulationSpeed * time);

            /*
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