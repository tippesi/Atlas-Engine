#ifndef AE_FOG_H
#define AE_FOG_H

#include "../System.h"

namespace Atlas {

	namespace Lighting {

		class Fog {

		public:
			Fog() = default;

			bool enable = true;

			vec3 color = vec3(0.5, 0.6, 0.7);

			float density = 0.05f;
			float height = 0.0f;
			float heightFalloff = 0.005f;

			float scatteringAnisotropy = 0.0f;

		};

	}

}

#endif