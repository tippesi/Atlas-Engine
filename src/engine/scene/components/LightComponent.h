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
				PointLight = 1,
				SpotLight = 2,
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
				float radius = 20.0f;
			};

			struct SpotLightProperties {
				vec3 position = vec3(0.0f);
				vec3 direction = -vec3(1.0f);
				float radius = 20.0f;
				float innerConeAngle = 1.0f;
				float outerConeAngle = 1.0f;
			};

			struct TypeProperties {
                TypeProperties() {
                    // Default to directional light
                    directional = DirectionalLightProperties();
                }

				TypeProperties(LightType type) {
					switch (type) {
					case LightType::PointLight: point = PointLightProperties(); break;
					case LightType::SpotLight: spot = SpotLightProperties(); break;
					default: directional = DirectionalLightProperties(); break;
					}
				}

				DirectionalLightProperties directional;
				PointLightProperties point;
				SpotLightProperties spot;
			};

			class LightComponent {

			public:
                LightComponent() = default;
				LightComponent(Scene* scene, const LightComponent& that);
				LightComponent(LightType type, LightMobility mobility = LightMobility::MovableLight);

				void AddDirectionalShadow(float distance, float bias, int32_t resolution, float edgeSoftness,
					int32_t cascadeCount, float splitCorrection, bool longRange = false, float longRangeDistance = 0.0f);

                void AddDirectionalShadow(float distance, float bias, int32_t resolution,
                    float edgeSoftness, vec3 shadowCenter, vec4 orthoSize);

                void AddPointShadow(float bias, int32_t resolution);

				void AddSpotShadow(float bias, int32_t resolution);

				bool IsVisible(const Volume::Frustum& frustum) const;

				LightType type = LightType::DirectionalLight;
				LightMobility mobility = LightMobility::MovableLight;

				vec3 color = vec3(1.0f);
				float intensity = 1.0f;
				float volumetricIntensity = 1.0f;

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