#ifndef AE_IMPOSTOR_H
#define AE_IMPOSTOR_H

#include "../System.h"
#include "../volume/AABB.h"
#include "../texture/Texture2DArray.h"


namespace Atlas {

	namespace Mesh {

		class Impostor {

		public:
			Impostor() {}

			Impostor(const Impostor& that);

			Impostor(int32_t views, int32_t resolution);

			Impostor& operator=(const Impostor& that);

			Texture::Texture2DArray diffuseTexture;
			Texture::Texture2DArray normalTexture;
			Texture::Texture2DArray specularTexture;

			Volume::AABB aabb;

			int32_t views = 1;
			int32_t resolution = 64;

			float cutoff = 0.7f;

		};

	}

}

#endif