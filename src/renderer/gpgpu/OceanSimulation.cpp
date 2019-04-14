#include "OceanSimulation.h"

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

				hT = Texture::Texture2D(N, N, AE_RGBA32F);

				h0.AddStage(AE_COMPUTE_STAGE, "ocean/h0.csh");
				ht.AddStage(AE_COMPUTE_STAGE, "ocean/ht.csh");

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

				state.h0K.Bind(GL_WRITE_ONLY, 0);
				state.h0MinusK.Bind(GL_WRITE_ONLY, 1);



			}

		}

	}

}