#pragma once

#include "../System.h"

namespace Atlas {

	namespace Common {

		namespace ColorConverter {

			static inline vec2 ConvertSRGBToLinear(vec2 color) {

				const float gamma = 2.2f;
				return glm::pow(color, vec2(gamma));

			}

			static inline vec3 ConvertSRGBToLinear(vec3 color) {

				const float gamma = 2.2f;
				return glm::pow(color, vec3(gamma));

			}

			static inline vec4 ConvertSRGBToLinear(vec4 color) {

				const float gamma = 2.2f;
				return vec4(glm::pow(vec3(color), vec3(gamma)), color.a);

			}

			static inline vec2 ConvertLinearToSRGB(vec2 color) {

				const float gamma = 1.0f / 2.2f;
				return glm::pow(color, vec2(gamma));

			}

			static inline vec3 ConvertLinearToSRGB(vec3 color) {

				const float gamma = 1.0f / 2.2f;
				return glm::pow(color, vec3(gamma));

			}

			static inline vec4 ConvertLinearToSRGB(vec4 color) {

				const float gamma = 1.0f / 2.2f;
				return vec4(glm::pow(vec3(color), vec3(gamma)), color.a);

			}

		}

	}

}