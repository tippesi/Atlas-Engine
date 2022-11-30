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

			struct Scattering {
				float extinctionFactor = 1.0;
				float scatteringFactor = 0.5;
				float eccentricity = 0.2f;
			};

			float shapeScale = 1.0f;
			float detailScale = 1.0f;
			float shapeSpeed = 1.0f;
			float detailSpeed = 0.5f;

			float densityMultiplier = 5.0f;

			float lowerHeightFalloff = 0.2f;
			float upperHeightFalloff = 0.25f;

			float silverLiningSpread = 0.05f;
			float silverLiningIntensity = 1.0f;

			Scattering scattering;

			vec3 aabbMin = vec3(0.0f,5.0f, 0.0f);
			vec3 aabbMax = vec3(10.0f);

			bool needsNoiseUpdate = true;

		};

	}

}

#endif