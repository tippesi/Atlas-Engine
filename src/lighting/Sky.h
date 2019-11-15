#ifndef AE_SKY_H
#define AE_SKY_H

#include "../System.h"
#include "../texture/Cubemap.h"
#include "DirectionalLight.h"

namespace Atlas {

	namespace Lighting {

		class Sky {

		public:
			Sky();

			DirectionalLight* sun = nullptr;

			Texture::Cubemap* cubemap = nullptr;

		};

	}

}


#endif