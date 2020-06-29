#ifndef AE_IRRADIANCEVOLUME_H
#define AE_IRRADIANCEVOLUME_H

#include "../System.h"

#include "../volume/AABB.h"
#include "../buffer/Buffer.h"
#include "../texture/Texture2DArray.h"
#include "../terrain/Terrain.h"

namespace Atlas {

	namespace Lighting {

		class IrradianceVolume {

		public:
			IrradianceVolume() {}

			IrradianceVolume(Volume::AABB aabb, ivec3 probeCount);

			ivec3 GetIrradianceArrayOffset(ivec3 probeIndex);

			ivec3 GetMomentsArrayOffset(ivec3 probeIndex);

			vec3 GetProbeLocation(ivec3 probeIndex);

			Volume::AABB aabb;
			ivec3 probeCount;

			vec3 size;
			vec3 cellSize;

			int32_t bounceCount = 0;

			int32_t irrRes = 16;
			int32_t momRes = 16;

			Texture::Texture2DArray irradianceArray;
			Texture::Texture2DArray momentsArray;

		};

	}

}

#endif