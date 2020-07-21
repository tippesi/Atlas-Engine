#include "OceanSimulation.h"
#include "../Clock.h"
#include "../buffer/Buffer.h"

#include <vector>

bool set = false;

namespace Atlas {

	namespace Ocean {

		OceanSimulation::OceanSimulation(int32_t N, int32_t L) : N(N), L(L) {

			noise0 = Texture::Texture2D(N, N, AE_R8);
			noise1 = Texture::Texture2D(N, N, AE_R8);
			noise2 = Texture::Texture2D(N, N, AE_R8);
			noise3 = Texture::Texture2D(N, N, AE_R8);			

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

			displacementMap = Texture::Texture2D(N, N, AE_RGBA16F, GL_REPEAT, GL_LINEAR, true, true);
			normalMap = Texture::Texture2D(N, N, AE_RGBA8, GL_REPEAT, GL_LINEAR, true, true);

			displacementMapPrev = Texture::Texture2D(N, N, AE_RGBA16F, GL_REPEAT, GL_LINEAR, true, true);

			h0K = Texture::Texture2D(N, N, AE_RGBA32F);

#ifdef AE_API_GLES
			twiddleIndices = Texture::Texture2D((int32_t)log2((float)N), N, AE_RGBA32F);
			hTDy = Texture::Texture2D(N, N, AE_RGBA32F);
			hTDyPingpong = Texture::Texture2D(N, N, AE_RGBA32F);
#else
			twiddleIndices = Texture::Texture2D((int32_t)log2((float)N), N, AE_RG32F);
			hTDy = Texture::Texture2D(N, N, AE_RG32F);
			hTDyPingpong = Texture::Texture2D(N, N, AE_RG32F);
#endif

			hTDxz = Texture::Texture2D(N, N, AE_RGBA32F);			
			hTDxzPingpong = Texture::Texture2D(N, N, AE_RGBA32F);

			h0.AddStage(AE_COMPUTE_STAGE, "ocean/h0.csh");
			ht.AddStage(AE_COMPUTE_STAGE, "ocean/ht.csh");
			twiddle.AddStage(AE_COMPUTE_STAGE, "ocean/twiddleIndices.csh");
			horizontalButterfly.AddStage(AE_COMPUTE_STAGE, "ocean/butterfly.csh");
			verticalButterfly.AddStage(AE_COMPUTE_STAGE, "ocean/butterfly.csh");
			choppyHorizontalButterfly.AddStage(AE_COMPUTE_STAGE, "ocean/butterfly.csh");
			choppyVerticalButterfly.AddStage(AE_COMPUTE_STAGE, "ocean/butterfly.csh");
			inversion.AddStage(AE_COMPUTE_STAGE, "ocean/inversion.csh");
			normal.AddStage(AE_COMPUTE_STAGE, "ocean/normal.csh");

			horizontalButterfly.AddMacro("HORIZONTAL");
			verticalButterfly.AddMacro("VERTICAL");

			choppyHorizontalButterfly.AddMacro("HORIZONTAL");
			choppyHorizontalButterfly.AddMacro("CHOPPY");
			choppyVerticalButterfly.AddMacro("VERTICAL");
			choppyVerticalButterfly.AddMacro("CHOPPY");

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

			auto NUniform = h0.GetUniform("N");
			auto LUniform = h0.GetUniform("L");
			auto AUniform = h0.GetUniform("A");
			auto wUniform = h0.GetUniform("w");
			auto windspeedUniform = h0.GetUniform("windspeed");
			auto windDependencyUniform = h0.GetUniform("windDependency");

			h0.Bind();

			NUniform->SetValue(N);
			LUniform->SetValue(L);
			AUniform->SetValue(waveAmplitude);
			wUniform->SetValue(windDirection);
			windspeedUniform->SetValue(windSpeed);
			windDependencyUniform->SetValue(windDependency);

			noise0.Bind(GL_TEXTURE2);
			noise1.Bind(GL_TEXTURE3);
			noise2.Bind(GL_TEXTURE4);
			noise3.Bind(GL_TEXTURE5);

			h0K.Bind(GL_WRITE_ONLY, 0);

			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
				GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			glDispatchCompute(N / 16, N / 16, 1);

		}

		void OceanSimulation::ComputeTwiddleIndices() {

			auto nUniform = twiddle.GetUniform("N");

			auto bitCount = (int32_t)log2f((float)N);
			std::vector<int32_t> indices(N);

			for (int32_t i = 0; i < N; i++) {
				indices[i] = ReverseBits(i, bitCount);
			}

			Buffer::Buffer buffer(AE_SHADER_STORAGE_BUFFER, sizeof(int32_t), AE_BUFFER_DYNAMIC_STORAGE);
			buffer.SetSize(indices.size());
			buffer.SetData(indices.data(), 0, indices.size());

			nUniform->SetValue(N);

			twiddleIndices.Bind(GL_WRITE_ONLY, 0);
			buffer.BindBase(1);

			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
				GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			glDispatchCompute(bitCount, N / 16, 1);

		}

		void OceanSimulation::Compute(float deltaTime) {

			time += deltaTime;

			displacementMapPrev.Copy(displacementMap);

			ht.Bind();

			htNUniform->SetValue(N);
			htLUniform->SetValue(L);
			htTimeUniform->SetValue(simulationSpeed * time);

			hTDy.Bind(GL_WRITE_ONLY, 0);
			hTDxz.Bind(GL_WRITE_ONLY, 1);

			h0K.Bind(GL_READ_ONLY, 2);

			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
				GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			glDispatchCompute(N / 16, N / 16, 1);

			int32_t pingpong = 0;
			int32_t log2n = (int32_t)log2f((float)N);

			twiddleIndices.Bind(GL_READ_ONLY, 0);

			for (uint8_t j = 0; j < 4; j++) {

				// The first two passes calculate the height,
				// while the second two passes calculate choppyness.
				switch (j) {
				case 0: horizontalButterfly.Bind();  break;
				case 1: verticalButterfly.Bind(); break;
				case 2: choppyHorizontalButterfly.Bind(); break;
				case 3: choppyVerticalButterfly.Bind(); break;
				default: break;
				}

				butterflyNUniform->SetValue(N);

				for (int32_t i = 0; i < log2n; i++) {

					// Different binding points for the height and choppyness passes
					if (j < 2) {
						if (!pingpong) {
							hTDy.Bind(GL_READ_ONLY, 1);
							hTDyPingpong.Bind(GL_WRITE_ONLY, 2);
						}
						else {
							hTDyPingpong.Bind(GL_READ_ONLY, 1);
							hTDy.Bind(GL_WRITE_ONLY, 2);
						}
					}
					else {
						if (!pingpong) {
							hTDxz.Bind(GL_READ_ONLY, 3);
							hTDxzPingpong.Bind(GL_WRITE_ONLY, 4);
						}
						else {
							hTDxzPingpong.Bind(GL_READ_ONLY, 3);
							hTDxz.Bind(GL_WRITE_ONLY, 4);
						}
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

			// Inverse and correct the texture
			inversion.Bind();

			inversionNUniform->SetValue(N);
			inversionPingpongUniform->SetValue(pingpong);

			displacementMap.Bind(GL_WRITE_ONLY, 0);

            if (pingpong == 0) {
                hTDy.Bind(GL_READ_ONLY, 1);
                hTDxz.Bind(GL_READ_ONLY, 3);
            }
            else {
                hTDyPingpong.Bind(GL_READ_ONLY, 1);
                hTDxzPingpong.Bind(GL_READ_ONLY, 3);
            }

			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			glDispatchCompute(N / 16, N / 16, 1);

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

			normalMap.Bind();
			normalMap.GenerateMipmap();

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