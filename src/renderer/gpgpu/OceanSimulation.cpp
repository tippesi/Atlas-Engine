#include "OceanSimulation.h"
#include "../Clock.h"
#include "../buffer/Buffer.h"

#include <vector>

namespace Atlas {

	namespace Renderer {

		namespace GPGPU {

			OceanSimulation::OceanSimulation(int32_t N, int32_t L) : N(N), L(L) {

				noise0 = Texture::Texture2D(N, N, AE_R8);
				noise1 = Texture::Texture2D(N, N, AE_R8);
				noise2 = Texture::Texture2D(N, N, AE_R8);
				noise3 = Texture::Texture2D(N, N, AE_R8);

				twiddleIndices = Texture::Texture2D((int32_t)log2((float)N), N, AE_RGBA32F);

				Helper::NoiseGenerator::GenerateNoiseTexture2D(noise0);
				Helper::NoiseGenerator::GenerateNoiseTexture2D(noise1);
				Helper::NoiseGenerator::GenerateNoiseTexture2D(noise2);
				Helper::NoiseGenerator::GenerateNoiseTexture2D(noise3);

				hTDy = Texture::Texture2D(N, N, AE_RGBA32F);
				hTDx = Texture::Texture2D(N, N, AE_RGBA32F);
				hTDz = Texture::Texture2D(N, N, AE_RGBA32F);

				h0.AddStage(AE_COMPUTE_STAGE, "ocean/h0.csh");
				ht.AddStage(AE_COMPUTE_STAGE, "ocean/ht.csh");
				twiddle.AddStage(AE_COMPUTE_STAGE, "ocean/twiddleIndices.csh");

				NUniform = ht.GetUniform("N");
				LUniform = ht.GetUniform("L");
				timeUniform = ht.GetUniform("time");

				ComputeTwiddleIndices();

			}

			void OceanSimulation::AddOceanState(float waveAmplitude, vec2 waveDirection,
				float windSpeed) {

				OceanState state;

				state.h0K = Texture::Texture2D(N, N, AE_RGBA32F);
				state.h0MinusK = Texture::Texture2D(N, N, AE_RGBA32F);

				state.waveAmplitude = waveAmplitude;
				state.waveDirection = waveDirection;
				state.windSpeed = windSpeed;

				ComputeH0(state);

				oceanStates.push_back(state);

			}

			void OceanSimulation::ComputeH0(OceanState& state) {

				auto NUniform = h0.GetUniform("N");
				auto LUniform = h0.GetUniform("L");
				auto AUniform = h0.GetUniform("A");
				auto wUniform = h0.GetUniform("w");
				auto windspeedUniform = h0.GetUniform("windspeed");

				h0.Bind();

				NUniform->SetValue(N);
				LUniform->SetValue(L);
				AUniform->SetValue(state.waveAmplitude);
				wUniform->SetValue(state.waveDirection);
				windspeedUniform->SetValue(state.windSpeed);

				noise0.Bind(GL_TEXTURE2);
				noise1.Bind(GL_TEXTURE3);
				noise2.Bind(GL_TEXTURE4);
				noise3.Bind(GL_TEXTURE5);

				state.h0K.Bind(GL_WRITE_ONLY, 0);
				state.h0MinusK.Bind(GL_WRITE_ONLY, 1);

				glDispatchCompute(N / 16, N / 16, 1);

			}

			void OceanSimulation::ComputeTwiddleIndices() {

				auto nUniform = twiddle.GetUniform("N");

				auto bitCount = (int32_t)log2f((float)N);
				std::vector<int32_t> indices(N);

				for (int32_t i = 0; i < N; i++) {
					indices[i] = ReverseBits(i, bitCount);
				}

				Buffer::Buffer buffer(AE_SHADER_BUFFER, sizeof(int32_t), AE_BUFFER_DYNAMIC_STORAGE);
				buffer.SetSize(indices.size());
				buffer.SetData(indices.data(), 0, indices.size());

				nUniform->SetValue(N);

				twiddleIndices.Bind(GL_WRITE_ONLY, 0);
				buffer.BindBase(1);

				glDispatchCompute(bitCount, N / 16, 1);

			}

			void OceanSimulation::ComputeHT(OceanState& state) {

				ht.Bind();

				NUniform->SetValue(N);
				LUniform->SetValue(L);
				timeUniform->SetValue(Clock::Get());

				hTDy.Bind(GL_WRITE_ONLY, 0);
				hTDx.Bind(GL_WRITE_ONLY, 1);
				hTDz.Bind(GL_WRITE_ONLY, 2);

				state.h0K.Bind(GL_READ_ONLY, 3);
				state.h0MinusK.Bind(GL_READ_ONLY, 4);

				glDispatchCompute(N / 16, N / 16, 1);

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

}