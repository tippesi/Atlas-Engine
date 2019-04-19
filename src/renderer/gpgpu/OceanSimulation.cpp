#include "OceanSimulation.h"
#include "Engine.h"

namespace Atlas {

	namespace Renderer {

		namespace GPGPU {

			OceanSimulation::OceanSimulation(int32_t N, int32_t L) : N(N), L(L) {

				noise0 = Texture::Texture2D(N, N, AE_R8);
				noise1 = Texture::Texture2D(N, N, AE_R8);
				noise2 = Texture::Texture2D(N, N, AE_R8);
				noise3 = Texture::Texture2D(N, N, AE_R8);

				Helper::NoiseGenerator::GenerateNoiseTexture2D(noise0);
				Helper::NoiseGenerator::GenerateNoiseTexture2D(noise1);
				Helper::NoiseGenerator::GenerateNoiseTexture2D(noise2);
				Helper::NoiseGenerator::GenerateNoiseTexture2D(noise3);

				hTDy = Texture::Texture2D(N, N, AE_RGBA32F);
				hTDx = Texture::Texture2D(N, N, AE_RGBA32F);
				hTDz = Texture::Texture2D(N, N, AE_RGBA32F);

				h0.AddStage(AE_COMPUTE_STAGE, "ocean/h0.csh");
				ht.AddStage(AE_COMPUTE_STAGE, "ocean/ht.csh");

				NUniform = ht.GetUniform("N");
				LUniform = ht.GetUniform("L");
				timeUniform = ht.GetUniform("time");

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

				auto noise0Uniform = h0.GetUniform("noise0");
				auto noise1Uniform = h0.GetUniform("noise1");
				auto noise2Uniform = h0.GetUniform("noise2");
				auto noise3Uniform = h0.GetUniform("noise3");

				auto NUniform = h0.GetUniform("N");
				auto LUniform = h0.GetUniform("L");
				auto AUniform = h0.GetUniform("A");
				auto wUniform = h0.GetUniform("w");
				auto windspeedUniform = h0.GetUniform("windspeed");

				h0.Bind();

				noise0Uniform->SetValue(0);
				noise1Uniform->SetValue(1);
				noise2Uniform->SetValue(2);
				noise3Uniform->SetValue(3);

				NUniform->SetValue(N);
				LUniform->SetValue(L);
				AUniform->SetValue(state.waveAmplitude);
				wUniform->SetValue(state.waveDirection);
				windspeedUniform->SetValue(state.windSpeed);

				noise0.Bind(GL_TEXTURE0);
				noise1.Bind(GL_TEXTURE1);
				noise2.Bind(GL_TEXTURE2);
				noise3.Bind(GL_TEXTURE3);

				state.h0K.Bind(GL_WRITE_ONLY, 0);
				state.h0MinusK.Bind(GL_WRITE_ONLY, 1);

				glDispatchCompute(N / 16, N / 16, 1);

			}

			void OceanSimulation::ComputeHT(OceanState& state) {

				ht.Bind();

				NUniform->SetValue(N);
				LUniform->SetValue(L);
				timeUniform->SetValue(Engine::GetClock());

				hTDy.Bind(GL_WRITE_ONLY, 0);
				hTDx.Bind(GL_WRITE_ONLY, 1);
				hTDz.Bind(GL_WRITE_ONLY, 2);

				state.h0K.Bind(GL_READ_ONLY, 3);
				state.h0MinusK.Bind(GL_READ_ONLY, 4);

				glDispatchCompute(N / 16, N / 16, 1);

			}

		}

	}

}