#include "VolumetricClouds.h"

namespace Atlas {

	namespace Lighting {

		VolumetricClouds::VolumetricClouds(int32_t shapeResolution, int32_t detailResolution) {

			shapeTexture = Texture::Texture3D(shapeResolution, shapeResolution, shapeResolution,
				GL_RGBA16F, GL_REPEAT, GL_LINEAR);
			detailTexture = Texture::Texture3D(detailResolution, detailResolution, detailResolution,
				GL_RGBA16F, GL_REPEAT, GL_LINEAR);

		}

	}

}