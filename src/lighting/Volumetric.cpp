#include "Volumetric.h"

namespace Atlas {

	namespace Lighting {

		Volumetric::Volumetric(int32_t width, int32_t height, int32_t sampleCount, float scattering, float scatteringFactor) :
				sampleCount(sampleCount), scattering(scattering), scatteringFactor(scatteringFactor) {

			map = new Texture::Texture2D(width, height, AE_R16F, GL_CLAMP_TO_EDGE, GL_LINEAR, false, false);
			blurMap = new Texture::Texture2D(width, height, AE_R16F, GL_CLAMP_TO_EDGE, GL_LINEAR, false, false);

		}

		Volumetric::~Volumetric() {

			delete map;
			delete blurMap;

		}

	}

}