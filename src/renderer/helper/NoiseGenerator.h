#ifndef AE_NOISEGENERATOR_H
#define AE_NOISEGENERATOR_H

#include "../../System.h"
#include "../../texture/Texture2D.h"

namespace Atlas {

	namespace Renderer {

		namespace Helper {

			class NoiseGenerator {

			public:
				static void GenerateNoiseTexture2D(Texture::Texture2D& texture);

			};

		}

	}

}

#endif