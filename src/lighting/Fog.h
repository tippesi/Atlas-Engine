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

			float height = 0.0f;
			float scale = 0.005f;
			float distanceScale = 0.05f;

			float scatteringPower = 8.0f;

		};

	}

}

#endif