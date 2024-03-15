#pragma once

#include "../Entity.h"
#include "../../System.h"


#include "CameraComponent.h"
#include "TransformComponent.h"

#include "../../lighting/Shadow.h"

namespace Atlas {

	namespace Scene {

		class Scene;

		namespace Components {

			enum class LightType {
				DirectionalLight = 0,
				PointLight = 1
			};

			enum class LightMobility {
				StationaryLight = 0,
				MovableLight = 1
			};

			struct DirectionalLightProperties {
				vec3 direction = -vec3(1.0f);
			};

			struct PointLightProperties {
				vec3 position = vec3(0.0f);
				float radius = 10.0f;
				float attenuation = 1.0f;
			};

			union TypeProperties {
                TypeProperties() {
                    // Default to directional light
                    directional = DirectionalLightProperties();
                }

				TypeProperties(LightType type) {
					switch (type) {
					case LightType::PointLight: point = PointLightProperties(); break;
					default: directional = DirectionalLightProperties(); break;
					}
				}

				DirectionalLightProperties directional;
				PointLightProperties point;
			};

			class LightComponent {

			public:
                LightComponent() = default;
				LightComponent(const LightComponent& that) = default;
				LightComponent(LightType type, LightMobility mobility = LightMobility::MovableLight);

				void AddDirectionalShadow(float distance, float bias, int32_t resolution, float edgeSoftness,
					int32_t cascadeCount, float splitCorrection, bool longRange = false, float longRangeDistance = 0.0f);

                void AddDirectionalShadow(float distance, float bias, int32_t resolution,
                    float edgeSoftness, vec3 shadowCenter, vec4 orthoSize);

                void AddPointShadow(float bias, int32_t resolution);

				LightType type = LightType::DirectionalLight;
				LightMobility mobility = LightMobility::MovableLight;

				vec3 color = vec3(1.0f);
				float intensity = 1.0f;

				TypeProperties properties;
				TypeProperties transformedProperties;

				Ref<Lighting::Shadow> shadow = nullptr;

				bool isMain = false;
                bool volumetric = true;

			private:
				void Update(const TransformComponent* transform);

				void Update(const CameraComponent& camera);

				void UpdateShadowCascade(Lighting::ShadowView& cascade, const CameraComponent& camera);

				float FrustumSplitFormula(float correction, float nearDist, float farDist, float splitIndex, float splitCount);

				friend Scene;

			};

		}

	}

}