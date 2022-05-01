#include "Packing.h"

#include <glm/gtc/packing.hpp>

namespace Atlas {

	namespace Common {

		namespace Packing {

			uint32_t PackNormalizedFloat3x10_1x2(vec4 vector) {

				return glm::packSnorm3x10_1x2(vector);

			}


			vec4 UnpackNormalizedFloat3x10_1x2(uint32_t packed) {

				return glm::unpackSnorm3x10_1x2(packed);

			}

			int32_t PackSignedVector3x10_1x2(vec4 vector) {

				int32_t packed = 0;

				packed |= int32_t(((vector.x * 0.5f + 0.5f) * 1023.0f)) << 0;
				packed |= int32_t(((vector.y * 0.5f + 0.5f) * 1023.0f)) << 10;
				packed |= int32_t(((vector.z * 0.5f + 0.5f) * 1023.0f)) << 20;
				packed |= int32_t(((vector.w * 0.5f + 0.5f) * 2.0f)) << 30;

				return packed;

			}

			vec4 UnpackSignedVector3x10_1x2(int32_t packed) {

				vec4 vector;

				vector.x = float((packed >> 0) & 1023) / 1023.0f * 2.0f - 1.0f;
				vector.y = float((packed >> 10) & 1023) / 1023.0f * 2.0f - 1.0f;
				vector.z = float((packed >> 20) & 1023) / 1023.0f * 2.0f - 1.0f;
				vector.w = float((packed >> 30) & 3) - 1.0f;

				return vector;

			}

			int32_t PackUnsignedVector3x10_1x2(vec4 vector) {

				int32_t packed = 0;

				packed |= int32_t((vector.x * 1023.0f)) << 0;
				packed |= int32_t((vector.y * 1023.0f)) << 10;
				packed |= int32_t((vector.z * 1023.0f)) << 20;
				packed |= int32_t((vector.w * 3.0f)) << 30;

				return packed;

			}

			vec4 UnpackUnsignedVector3x10_1x2(int32_t packed) {

				vec4 vector;

				vector.x = float((packed >> 0) & 1023) / 1023.0f;
				vector.y = float((packed >> 10) & 1023) / 1023.0f;
				vector.z = float((packed >> 20) & 1023) / 1023.0f;
				vector.w = float((packed >> 30) & 3) / 3.0f;

				return vector;

			}

		}

	}

}