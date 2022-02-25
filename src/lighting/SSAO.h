#ifndef AE_SSAO_H
#define AE_SSAO_H

#include "../System.h"
#include "../RenderTarget.h"
#include "../texture/Texture2D.h"

namespace Atlas {

	namespace Lighting {

		class SSAO {

		public:
			SSAO(uint32_t sampleCount = 16);

			void SetSampleCount(uint32_t sampleCount);

			uint32_t sampleCount = 16;

			float radius = 3.0f;
			float strength = 2.0f;

			bool enable = true;

			Texture::Texture2D noiseTexture;
			std::vector<vec3> samples;

		};

	}

}


#endif