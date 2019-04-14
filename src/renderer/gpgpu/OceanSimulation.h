#ifndef AE_OCEANSIMULATION_H
#define AE_OCEANSIMULATION_H

#include "../../System.h"
#include "../helper/NoiseGenerator.h"
#include "../../texture/Texture2D.h"
#include "../../shader/Shader.h"

#include <vector>

namespace Atlas {

	namespace Renderer {

		namespace GPGPU {

			class OceanSimulation {
			
			public:
				OceanSimulation(int32_t N, int32_t L);

				void AddOceanState(float waveAmplitude, vec2 waveDirection,
					float windSpeed);

			private:
				struct OceanState {

					Texture::Texture2D h0K;
					Texture::Texture2D h0MinusK;

					float waveAmplitude;
					vec2 waveDirection;
					float windSpeed;

				};

				void ComputeH0(OceanState& state);

				int32_t N;
				int32_t L;

				Shader::Shader h0;
				Shader::Shader ht;

				// Precomputed noise textures
				Texture::Texture2D noise0;
				Texture::Texture2D noise1;
				Texture::Texture2D noise2;
				Texture::Texture2D noise3;

				Texture::Texture2D hT;

				std::vector<OceanState> oceanStates;

			};

		}

	}

}


#endif