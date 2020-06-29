#include "Volumetric.h"

namespace Atlas {

	namespace Lighting {

		Volumetric::Volumetric(int32_t width, int32_t height, int32_t sampleCount) : sampleCount(sampleCount) {

			map = Texture::Texture2D(width, height, AE_R16F, GL_CLAMP_TO_EDGE, GL_LINEAR, false, false);
			blurMap = Texture::Texture2D(width, height, AE_R16F, GL_CLAMP_TO_EDGE, GL_LINEAR, false, false);

		}

	}

}