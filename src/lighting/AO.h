#ifndef AE_AO_H
#define AE_AO_H

#include "../System.h"
#include "../RenderTarget.h"
#include "../texture/Texture2D.h"

namespace Atlas {

	namespace Lighting {

		class AO {

		public:
			AO(int32_t sampleCount = 16);

			void SetSampleCount(int32_t sampleCount);

			int32_t sampleCount = 16;

			float radius = 3.0f;
			float strength = 1.0f;

			bool enable = true;
			bool rt = false;

			Texture::Texture2D noiseTexture;
			std::vector<vec3> samples;

		};

	}

}


#endif