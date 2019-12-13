#include "Impostor.h"

namespace Atlas {

	namespace Mesh {

		Impostor::Impostor(int32_t views, int32_t resolution) : 
			views(views), resolution(resolution) {

			diffuseTexture = Atlas::Texture::Texture2DArray(resolution,
				resolution, views, AE_RGBA8, GL_CLAMP_TO_EDGE,
				GL_LINEAR, true, true);
			normalTexture = Atlas::Texture::Texture2DArray(resolution,
				resolution, views, AE_RGB8, GL_CLAMP_TO_EDGE,
				GL_LINEAR, true, true);

		}

	}

}