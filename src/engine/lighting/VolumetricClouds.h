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
				float extinctionFactor = 0.24f;
				float scatteringFactor = 1.00f;

                float eccentricityFirstPhase = 0.0f;
                float eccentricitySecondPhase = -0.5f;
                float phaseAlpha = 0.5f;
			};

			int32_t sampleCount = 32;
			int32_t shadowSampleCount = 5;

			float minHeight = 100.0f;
			float maxHeight = 600.0f;
			float distanceLimit = 3000.0f;

			float shapeScale = 1.0f;
			float detailScale = 16.0f;
			float shapeSpeed = 1.0f;
			float detailSpeed = 10.0f;
			float detailStrength = 0.15f;

			float densityMultiplier = 0.8f;

			float heightStretch = 0.5f;

			float darkEdgeFocus = 16.0f;
			float darkEdgeAmbient = 0.3f;

			Scattering scattering;

			bool needsNoiseUpdate = true;
			bool enable = true;

		};

	}

}

#endif