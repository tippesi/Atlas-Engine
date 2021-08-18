#ifndef AE_DIRECTIONALLIGHT_H
#define AE_DIRECTIONALLIGHT_H

#include "../System.h"
#include "Light.h"

namespace Atlas {

	namespace Lighting {

		class DirectionalLight : public Light {

		public:
			DirectionalLight(int32_t mobility = AE_MOVABLE_LIGHT);

			~DirectionalLight();

			void AddShadow(float distance, float bias, int32_t resolution, int32_t cascadeCount, float splitCorrection);

			void AddShadow(float distance, float bias, int32_t resolution, vec3 centerPoint, mat4 orthoProjection);

			void AddLongRangeShadow(float distance);

			void RemoveShadow() override;

			void AddVolumetric(int32_t width, int32_t height, int32_t sampleCount = 10, float intensity = 1.0f);

			void RemoveVolumetric() override;

			void Update(Camera* camera) override;

			vec3 direction = vec3(0.0f, -1.0f, 0.0f);

		private:
			void UpdateShadowCascade(ShadowComponent* cascade, Camera* camera);

			float FrustumSplitFormula(float correction, float nearDist, float farDist, float splitIndex, float splitCount);

			vec3 shadowCenter;
			bool useShadowCenter = true;

		};


	}

}

#endif