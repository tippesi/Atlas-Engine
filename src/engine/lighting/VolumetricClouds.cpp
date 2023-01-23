#include "VolumetricClouds.h"

namespace Atlas {

	namespace Lighting {

		VolumetricClouds::VolumetricClouds(int32_t shapeResolution, int32_t detailResolution) :
			shapeTexture(shapeResolution, shapeResolution, shapeResolution,
				GL_RGBA16F, GL_REPEAT, GL_LINEAR_MIPMAP_LINEAR, false, true),
			detailTexture(detailResolution, detailResolution, detailResolution,
				GL_RGBA16F, GL_REPEAT, GL_LINEAR_MIPMAP_LINEAR, false, true) {



		}

	}

}