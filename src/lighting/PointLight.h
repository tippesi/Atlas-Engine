#ifndef AE_POINTLIGHT_H
#define AE_POINTLIGHT_H

#include "../System.h"
#include "Light.h"

namespace Atlas {

	namespace Lighting {

		class PointLight : public Light {

		public:
			PointLight(int32_t mobility = AE_STATIONARY_LIGHT);

			~PointLight();

			void AddShadow(float bias, int32_t resolution);

			void RemoveShadow();

			void AddVolumetric(int32_t width, int32_t height, int32_t sampleCount, float scattering, float scatteringFactor = 1.0f);

			void RemoveVolumetric();

			void Update(Camera* camera);

			float GetRadius();

			vec3 location;

			float attenuation;

		private:
			float radius;

		};


	}

}

#endif
