#include "VolumetricClouds.h"

namespace Atlas {

	namespace Lighting {

		VolumetricClouds::VolumetricClouds(int32_t shapeResolution, int32_t detailResolution) :
			shapeTexture(shapeResolution, shapeResolution, shapeResolution,
				VK_FORMAT_R16G16B16A16_SFLOAT, Texture::Wrapping::Repeat, Texture::Filtering::MipMapLinear),
			detailTexture(detailResolution, detailResolution, detailResolution,
                VK_FORMAT_R16G16B16A16_SFLOAT, Texture::Wrapping::Repeat, Texture::Filtering::MipMapLinear)  {



		}

	}

}