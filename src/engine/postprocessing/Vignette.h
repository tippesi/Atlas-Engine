#ifndef AE_VIGNETTE_H
#define AE_VIGNETTE_H

#include "../System.h"

namespace Atlas {

	namespace PostProcessing {

		class Vignette {

		public:
			Vignette() = default;

			///
			/// \param offset
			/// \param power
			/// \param strength
			/// \param color
			Vignette(float offset, float power, float strength, vec3 color = vec3(0.0f)) :
					enable(true), offset(offset), power(power), strength(strength), color(color) { };

			bool enable = false;

			float offset;
			float power;
			float strength;

			vec3 color;

		};

	}

}

#endif