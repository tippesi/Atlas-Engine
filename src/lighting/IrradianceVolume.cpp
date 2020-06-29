#include "IrradianceVolume.h"

namespace Atlas {

	namespace Lighting {

		IrradianceVolume::IrradianceVolume(Volume::AABB aabb, ivec3 probeCount) :
			aabb(aabb), probeCount(probeCount) {

			auto irrRes = ivec2(this->irrRes + 2);
			irrRes.x *= probeCount.x;
			irrRes.y *= probeCount.z;

			auto momRes = ivec2(this->momRes + 2);
			momRes.x *= probeCount.x;
			momRes.y *= probeCount.z;

			size = aabb.max - aabb.min;
			cellSize = size / vec3(probeCount - ivec3(1));

			irradianceArray = Texture::Texture2DArray(irrRes.x, irrRes.y, probeCount.y, AE_RGBA16F,
				GL_CLAMP_TO_EDGE, GL_LINEAR);

			momentsArray = Texture::Texture2DArray(momRes.x, momRes.y, probeCount.y, AE_RG16F,
				GL_CLAMP_TO_EDGE, GL_LINEAR);

			// We set the data because the border pixels need to be valid
			std::vector<float> irrVector(irrRes.x * irrRes.y * 4);
			std::vector<float> momVector(momRes.x * momRes.y * 2);

			std::fill(irrVector.begin(), irrVector.end(), 0.0f);
			std::fill(momVector.begin(), momVector.end(), 0.0f);

			for (int32_t i = 0; i < probeCount.y; i++) {

				irradianceArray.SetData(irrVector, i);
				momentsArray.SetData(momVector, i);

			}
			   
		}

		ivec3 IrradianceVolume::GetIrradianceArrayOffset(ivec3 probeIndex) {

			auto irrRes = ivec2(this->irrRes + 2);

			return ivec3(probeIndex.x * irrRes.x + 1,
				probeIndex.z * irrRes.y + 1, probeIndex.y);

		}

		ivec3 IrradianceVolume::GetMomentsArrayOffset(ivec3 probeIndex) {

			auto momRes = ivec2(this->momRes + 2);

			return ivec3(probeIndex.x * momRes.x + 1,
				probeIndex.z * momRes.y + 1, probeIndex.y);

		}

		vec3 IrradianceVolume::GetProbeLocation(ivec3 probeIndex) {

			return aabb.min + vec3(probeIndex) * cellSize;

		}

	}

}