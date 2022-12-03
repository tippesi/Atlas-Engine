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
				float extinctionFactor = 0.33f;
				float scatteringFactor = 1.25;
				float eccentricity = 0.0f;
			};

			float minHeight = 100.0f;
			float maxHeight = 400.0f;
			float distanceLimit = 2000.0f;

			float shapeScale = 1.0f;
			float detailScale = 16.0f;
			float shapeSpeed = 1.0f;
			float detailSpeed = 10.0f;
			float detailStrength = 0.3f;

			float densityMultiplier = 0.7f;

			float lowerHeightFalloff = 0.1f;
			float upperHeightFalloff = 0.67f;

			float silverLiningSpread = 0.24f;
			float silverLiningIntensity = 0.077f;

			Scattering scattering;

			bool needsNoiseUpdate = true;
			bool enable = true;

		};

	}

}

#endif