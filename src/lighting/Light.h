#ifndef AE_ILIGHT_H
#define AE_ILIGHT_H

#include "../System.h"
#include "Shadow.h"
#include "Volumetric.h"

#define AE_STATIONARY_LIGHT 0
#define AE_MOVABLE_LIGHT 1

#define AE_DIRECTIONAL_LIGHT 0
#define AE_POINT_LIGHT 1
#define AE_SPOT_LIGHT 2

namespace Atlas {

	namespace Lighting {

		class Light {

		public:
			virtual void RemoveShadow() = 0;

			virtual void RemoveVolumetric() = 0;

			virtual void Update(Camera* camera) = 0;

			inline Shadow* GetShadow() {
				return shadow;
			}

			inline Volumetric* GetVolumetric() {
				return volumetric;
			}

			int32_t type;
			int32_t mobility;

			vec3 color;
			float ambient;

		protected:
			Light(int32_t type, int32_t mobility) : type(type), mobility(mobility) {}

			Shadow* shadow;
			Volumetric* volumetric;

		};

	}

}


#endif
