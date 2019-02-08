#ifndef AE_SKYBOX_H
#define AE_SKYBOX_H

#include "../System.h"
#include "../texture/Cubemap.h"

namespace Atlas {

	namespace Lighting {

		class Skybox {

		public:
			Skybox(Texture::Cubemap* cubemap, mat4 matrix = mat4(1.0f)) : cubemap(cubemap), matrix(matrix) {};

			Texture::Cubemap * cubemap;
			mat4 matrix;

		};

	}

}

#endif