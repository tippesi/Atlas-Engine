#ifndef AE_SSAO_H
#define AE_SSAO_H

#include "../System.h"
#include "../texture/Texture2D.h"

namespace Atlas {

	namespace Lighting {

		class SSAO {

		public:
			SSAO(int32_t width, int32_t height, uint32_t sampleCount = 16);

			void SetSampleCount(uint32_t sampleCount);

			uint32_t sampleCount = 16;

			float radius = 3.0f;
			float strength = 2.0f;

			bool enable = true;

			Texture::Texture2D noiseTexture;
			std::vector<vec3> samples;

			Texture::Texture2D map;
			Texture::Texture2D blurMap;

		};

	}

}


#endif