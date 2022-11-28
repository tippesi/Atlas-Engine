#ifndef AE_VOLUMETRICCLOUDS_H
#define AE_VOLUMETRICCLOUDS_H

#include "../System.h"
#include "../RenderTarget.h"
#include "../texture/Texture3D.h"

namespace Atlas {

	namespace Lighting {

		class VolumetricClouds {

		public:
			VolumetricClouds(int32_t shapeResolution = 128, int32_t detailResolution = 32);
			
			Texture::Texture3D shapeTexture;
			Texture::Texture3D detailTexture;

			float shapeScale = 1.0f;
			float detailScale = 1.0f;

			float densityMultiplier = 5.0f;
			float densityCutoff = 0.5f;

			vec3 aabbMin = vec3(0.0f,5.0f, 0.0f);
			vec3 aabbMax = vec3(10.0f);

			bool needsNoiseUpdate = true;

		};

	}

}

#endif