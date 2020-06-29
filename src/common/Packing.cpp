#include "Packing.h"

namespace Atlas {

	namespace Common {

		namespace Packing {

			uint32_t PackNormalizedFloat3x10_1x2(vec4 vector) {

				const uint32_t xs = vector.x < 0;
				const uint32_t ys = vector.y < 0;
				const uint32_t zs = vector.z < 0;
				const uint32_t ws = vector.w < 0;

				return uint32_t
				(
					ws << 31 | ((uint32_t)(vector.w + (ws << 1)) & 1) << 30 |
					zs << 29 | ((uint32_t)(vector.z * 511 + (zs << 9)) & 511) << 20 |
					ys << 19 | ((uint32_t)(vector.y * 511 + (ys << 9)) & 511) << 10 |
					xs << 9 | ((uint32_t)(vector.x * 511 + (xs << 9)) & 511)
				);

			}


			vec4 UnpackNormalizedFloat3x10_1x2(uint32_t packed) {

				vec4 vector;

				uint32_t x = (packed >> 9) & 1;
				uint32_t y = (packed >> 19) & 1;
				uint32_t z = (packed >> 29) & 1;
				uint32_t w = (packed >> 31) & 1;

				int32_t xs = (x > 0 ? ((packed & 511) | ~511) : (packed & 511));
				int32_t ys = (y > 0 ? (((packed >> 10) & 511) | ~511) : ((packed >> 10) & 511));
				int32_t zs = (z > 0 ? (((packed >> 20) & 511) | ~511) : ((packed >> 20) & 511));
				int32_t ws = (w > 0 ? (((packed >> 30) & 1) | ~1) : ((packed >> 30) & 1));

				vector.x = (float)xs / (x > 0 ? 512.0f : 511.0f);
				vector.y = (float)ys / (y > 0 ? 512.0f : 511.0f);
				vector.z = (float)zs / (z > 0 ? 512.0f : 511.0f);
				vector.w = (float)ws;

				return vector;

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