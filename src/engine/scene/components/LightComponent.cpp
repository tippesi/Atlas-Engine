#include "LightComponent.h"

namespace Atlas {

	namespace Scene {

		namespace Components {

            const mat4 clipMatrix = mat4(1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, -1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.5f, 0.0f,
                0.0f, 0.0f, 0.5f, 1.0f);

			LightComponent::LightComponent(LightType type, LightMobility mobility) : 
				type(type), mobility(mobility), properties(type), transformedProperties(type) {



			}

			void LightComponent::AddDirectionalShadow(float distance, float bias, int32_t resolution,
                int32_t cascadeCount, float splitCorrection, bool longRange, float longRangeDistance) {

				AE_ASSERT(type == LightType::DirectionalLight && "Component must be of type directional light");

                shadow = CreateRef<Lighting::Shadow>(distance, bias, resolution, 
                    glm::min(cascadeCount, MAX_SHADOW_VIEW_COUNT), splitCorrection);

                shadow->allowTerrain = true;

                if (longRange) {
                    auto& component = shadow->components.back();

                    component.farDistance = longRangeDistance;
                    component.nearDistance = shadow->components[shadow->components.size() - 1].farDistance;

                    shadow->longRangeDistance = longRangeDistance;
                    shadow->longRange = true;
                }

			}

            void LightComponent::AddDirectionalShadow(float distance, float bias, int32_t resolution,
                vec3 shadowCenter, mat4 orthoProjection) {

                AE_ASSERT(type == LightType::DirectionalLight && "Component must be of type directional light");

                shadow = CreateRef<Lighting::Shadow>(distance, bias, resolution);

                shadow->center = shadowCenter;

                shadow->allowTerrain = true;

                shadow->components[0].nearDistance = 0.0f;
                shadow->components[0].farDistance = distance;
                shadow->components[0].projectionMatrix = clipMatrix * orthoProjection;
                shadow->components[0].frustumMatrix = clipMatrix * orthoProjection;
                shadow->components[0].terrainFrustumMatrix = clipMatrix * orthoProjection;
                shadow->components[0].viewMatrix = glm::lookAt(shadowCenter, shadowCenter +
                    properties.directional.direction, vec3(0.0f, 1.0f, 0.0f));

            }

			void LightComponent::AddPointShadow(float bias, int32_t resolution) {

				AE_ASSERT(type == LightType::PointLight && "Component must be of type point light");

                shadow = CreateRef<Lighting::Shadow>(0.0f, bias, resolution, true);

			}

            void LightComponent::Update(const TransformComponent* transform) {

                transformedProperties = properties;

                if (type == LightType::DirectionalLight && transform) {
                    transformedProperties.directional.direction = glm::normalize(vec3(transform->globalMatrix *
                        vec4(properties.directional.direction, 0.0f)));
                }
                else if (type == LightType::PointLight && transform) {
                    transformedProperties.point.position = vec3(transform->globalMatrix * vec4(properties.point.position, 1.0f));
                }

            }

            void LightComponent::Update(const CameraComponent& camera) {

                if (!shadow)
                    return;

                if (shadow->isCascaded && type == LightType::DirectionalLight) {
                    auto distance = shadow->distance;
                    auto componentCount = shadow->longRange ? shadow->componentCount - 1
                        : shadow->componentCount;

                    // We want cascaded shadow mapping for directional lights
                    for (int32_t i = 0; i < componentCount; i++) {
                        shadow->components[i].nearDistance = FrustumSplitFormula(shadow->splitCorrection,
                            camera.nearPlane, distance,
                            (float)i, (float)componentCount);
                        shadow->components[i].farDistance = FrustumSplitFormula(shadow->splitCorrection,
                            camera.nearPlane, distance,
                            (float)i + 1, (float)componentCount);
                    }

                    for (int32_t i = 0; i < shadow->componentCount; i++) {
                        UpdateShadowCascade(shadow->components[i], camera);
                    }

                }
                else if (!shadow->isCascaded && type == LightType::DirectionalLight) {
                    shadow->components[0].viewMatrix = glm::lookAt(shadow->center,
                        shadow->center + transformedProperties.directional.direction, vec3(0.0f, 1.0f, 0.0f));
                }
                else if (type == LightType::PointLight) {
                    vec3 position = transformedProperties.point.position;

                    mat4 projectionMatrix = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, properties.point.radius);
                    const vec3 faces[] = { vec3(1.0f, 0.0f, 0.0f), vec3(-1.0f, 0.0f, 0.0f),
                                     vec3(0.0f, 1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f),
                                     vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 0.0f, -1.0f) };

                    const vec3 ups[] = { vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f),
                                   vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 0.0f, -1.0f),
                                   vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f) };

                    for (uint8_t i = 0; i < 6; i++) {
                        auto viewMatrix = glm::lookAt(position, position + faces[i], ups[i]);
                        shadow->components[i].projectionMatrix = projectionMatrix;
                        shadow->components[i].viewMatrix = viewMatrix;
                        shadow->components[i].frustumMatrix = projectionMatrix * viewMatrix;
                    }
                }

                if (mobility == LightMobility::MovableLight)
                    shadow->Update();

            }

            void LightComponent::UpdateShadowCascade(Lighting::ShadowView& cascade, const CameraComponent& camera) {

                auto cameraLocation = camera.GetLocation();

                auto cascadeCenter = cameraLocation + camera.direction *
                    (cascade.nearDistance + (cascade.farDistance +
                        shadow->cascadeBlendDistance - cascade.nearDistance) * 0.5f);

                vec3 lightDirection = normalize(properties.directional.direction);

                // A near enough up vector. This is because if the light location is
                // (0.0f, 1.0f, 0.0f) the shadows wouldn't render correctly due to the
                // shadows (or lights) view matrix. This is just a hack
                vec3 up = glm::vec3(0.0000000000000001f, 1.0f, 0.0000000000000001f);
                cascade.viewMatrix = glm::lookAt(cascadeCenter, cascadeCenter + lightDirection, up);

                std::vector<vec3> corners = camera.GetFrustumCorners(cascade.nearDistance,
                    cascade.farDistance + shadow->cascadeBlendDistance);

                vec3 maxProj = vec3(cascade.viewMatrix * vec4(corners.at(0), 1.0f));
                vec3 minProj = maxProj;

                auto maxLength = 0.0f;

                for (auto corner : corners) {

                    maxLength = glm::max(maxLength, glm::length(corner - cascadeCenter));

                    corner = vec3(cascade.viewMatrix * vec4(corner, 1.0f));

                    maxProj.x = glm::max(maxProj.x, corner.x);
                    maxProj.y = glm::max(maxProj.y, corner.y);
                    maxProj.z = glm::max(maxProj.z, corner.z);

                    minProj.x = glm::min(minProj.x, corner.x);
                    minProj.y = glm::min(minProj.y, corner.y);
                    minProj.z = glm::min(minProj.z, corner.z);
                }

                // Tighter frustum for normal meshes
                cascade.frustumMatrix = glm::ortho(minProj.x,
                    maxProj.x,
                    minProj.y,
                    maxProj.y,
                    -maxProj.z - 300.0f, // We need to render stuff behind the camera
                    -minProj.z + 10.0f) * cascade.viewMatrix; // We need to extend a bit to hide seams at cascade splits

                cascade.terrainFrustumMatrix = glm::ortho(minProj.x,
                    maxProj.x,
                    minProj.y,
                    maxProj.y,
                    -maxProj.z - 1250.0f, // We need to render stuff behind the camera
                    -minProj.z + 10.0f) * cascade.viewMatrix; // We need to extend a bit to hide seams at cascade splits

                maxLength = glm::ceil(maxLength);

                cascade.projectionMatrix = glm::ortho(-maxLength,
                    maxLength,
                    -maxLength,
                    maxLength,
                    -maxLength - 1250.0f, // We need to render stuff behind the camera
                    maxLength + 10.0f); // We need to extend a bit to hide seams at cascade splits

                glm::mat4 shadowMatrix = cascade.projectionMatrix * cascade.viewMatrix;
                glm::vec4 shadowOrigin = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
                shadowOrigin = shadowMatrix * shadowOrigin;
                shadowOrigin = shadowOrigin * (float)shadow->resolution / 2.0f;

                glm::vec4 roundedOrigin = glm::round(shadowOrigin);
                glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
                roundOffset = roundOffset * 2.0f / (float)shadow->resolution;
                roundOffset.z = 0.0f;
                roundOffset.w = 0.0f;

                glm::mat4 shadowProj = cascade.projectionMatrix;
                shadowProj[3] += roundOffset;
                cascade.projectionMatrix = clipMatrix * shadowProj;
                cascade.frustumMatrix = clipMatrix * cascade.frustumMatrix;
                cascade.terrainFrustumMatrix = clipMatrix * cascade.terrainFrustumMatrix;

            }

            float LightComponent::FrustumSplitFormula(float correction, float nearDist, float farDist, float splitIndex, float splitCount) {

                return correction * nearDist * powf(farDist / nearDist, splitIndex / splitCount) +
                    (1.0f - correction) * (nearDist + (splitIndex / splitCount) * (farDist - nearDist));

            }

		}

	}

}