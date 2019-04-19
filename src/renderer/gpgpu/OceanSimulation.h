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

			struct OceanState {

				Texture::Texture2D h0K;
				Texture::Texture2D h0MinusK;

				float waveAmplitude;
				vec2 waveDirection;
				float windSpeed;

			};

			class OceanSimulation {
			
			public:
				OceanSimulation(int32_t N, int32_t L);

				void AddOceanState(float waveAmplitude, vec2 waveDirection,
					float windSpeed);

				void ComputeHT(OceanState& state);

				std::vector<OceanState> oceanStates;

				Texture::Texture2D hTDy;
				Texture::Texture2D hTDx;
				Texture::Texture2D hTDz;

			private:
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

				Shader::Uniform* NUniform;
				Shader::Uniform* LUniform;
				Shader::Uniform* timeUniform;

			};

		}

	}

}


#endif