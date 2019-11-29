#ifndef AE_IMPOSTOR_H
#define AE_IMPOSTOR_H

#include "../System.h"
#include "../texture/Texture2DArray.h"


namespace Atlas {

	namespace Mesh {

		class Impostor {

		public:
			Impostor() {}

			Impostor(const Impostor& that);

			Impostor(int32_t views);

			Impostor& operator=(const Impostor& that);

			Texture::Texture2DArray diffuseTexture;
			Texture::Texture2DArray normalTexture;

		private:
			int32_t views = 1;

		};

	}

}

#endif