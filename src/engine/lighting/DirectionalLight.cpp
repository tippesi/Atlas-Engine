#include "DirectionalLight.h"

namespace Atlas {

    namespace Lighting {

        DirectionalLight::DirectionalLight(int32_t mobility) : Light(AE_DIRECTIONAL_LIGHT, mobility) {



        }

        DirectionalLight::~DirectionalLight() {

            if (shadow) delete shadow;

        }

        void DirectionalLight::AddShadow(float distance, float bias, int32_t resolution, int32_t cascadeCount, float splitCorrection) {

            if (shadow)
                delete shadow;

            shadow = new Shadow(distance, bias, resolution, glm::min(cascadeCount, MAX_SHADOW_CASCADE_COUNT), splitCorrection);

            useShadowCenter = false;

            shadow->allowTerrain = true;

        }

        void DirectionalLight::AddShadow(float distance, float bias, int32_t resolution, vec3 centerPoint, mat4 orthoProjection) {

            if (shadow)
                delete shadow;

            shadow = new Shadow(distance, bias, resolution);

            useShadowCenter = true;
            shadowCenter = centerPoint;

            shadow->allowTerrain = true;

            const mat4 clip = mat4(1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, -1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.5f, 0.0f,
                0.0f, 0.0f, 0.5f, 1.0f);

            shadow->components[0].nearDistance = 0.0f;
            shadow->components[0].farDistance = distance;
            shadow->components[0].projectionMatrix = clip * orthoProjection;
            shadow->components[0].frustumMatrix = clip * orthoProjection;
            shadow->components[0].terrainFrustumMatrix = clip * orthoProjection;
            shadow->components[0].viewMatrix = glm::lookAt(centerPoint, centerPoint + direction, vec3(0.0f, 1.0f, 0.0f));

        }

        void DirectionalLight::AddLongRangeShadow(float distance) {

            shadow->maps.Resize(shadow->resolution, shadow->resolution,
                                shadow->maps.depth + 1);

            shadow->componentCount += 1;

            auto component = ShadowComponent();

            component.farDistance = distance;
            component.nearDistance = shadow->components[shadow->components.size() - 1].farDistance;

            shadow->components.push_back(component);

            shadow->longRangeDistance = distance;
            shadow->longRange = true;

        }

        void DirectionalLight::RemoveShadow() {

            delete shadow;
            shadow = nullptr;

        }

        void DirectionalLight::AddVolumetric(int32_t sampleCount, float intensity) {

            volumetric = new Volumetric(sampleCount, intensity);

        }

        void DirectionalLight::RemoveVolumetric() {

            delete volumetric;
            volumetric = nullptr;

        }

        void DirectionalLight::Update(Camera* camera) {

            if (shadow != nullptr) {

                if (!useShadowCenter) {

                    auto distance = shadow->distance;
                    auto componentCount = shadow->longRange ? shadow->componentCount - 1
                        : shadow->componentCount;

                    // We want cascaded shadow mapping for directional lights
                    for (int32_t i = 0; i < componentCount; i++) {
                        shadow->components[i].nearDistance = FrustumSplitFormula(shadow->splitCorrection, 
                            camera->nearPlane, distance,
                            (float)i, (float)componentCount);
                        shadow->components[i].farDistance = FrustumSplitFormula(shadow->splitCorrection, 
                            camera->nearPlane, distance,
                            (float)i + 1, (float)componentCount);
                    }

                    for (int32_t i = 0; i < shadow->componentCount; i++) {
                        UpdateShadowCascade(&shadow->components[i], camera);
                    }

                }
                else {
                    shadow->components[0].viewMatrix = glm::lookAt(shadowCenter, shadowCenter + direction, vec3(0.0f, 1.0f, 0.0f));
                }

                if (mobility == AE_MOVABLE_LIGHT) {
                    shadow->Update();
                }

            }

        }

        void DirectionalLight::UpdateShadowCascade(ShadowComponent* cascade, Camera* camera) {

            auto cameraLocation = camera->GetLocation();

            auto cascadeCenter = cameraLocation + camera->direction * 
                (cascade->nearDistance + (cascade->farDistance + 
                    shadow->cascadeBlendDistance - cascade->nearDistance) * 0.5f);

            vec3 lightDirection = normalize(direction);

            // A near enough up vector. This is because if the light location is
            // (0.0f, 1.0f, 0.0f) the shadows wouldn't render correctly due to the
            // shadows (or lights) view matrix. This is just a hack
            vec3 up = glm::vec3(0.0000000000000001f, 1.0f, 0.0000000000000001f);
            cascade->viewMatrix = lookAt(cascadeCenter - lightDirection, cascadeCenter, up);

            std::vector<vec3> corners = camera->GetFrustumCorners(cascade->nearDistance, 
                cascade->farDistance + shadow->cascadeBlendDistance);

            vec3 maxProj = vec3(cascade->viewMatrix * vec4(corners.at(0), 1.0f));
            vec3 minProj = maxProj;

            auto maxLength = 0.0f;

            for (auto corner : corners) {

                maxLength = glm::max(maxLength, glm::length(corner - cascadeCenter));

                corner = vec3(cascade->viewMatrix * vec4(corner, 1.0f));

                maxProj.x = glm::max(maxProj.x, corner.x);
                maxProj.y = glm::max(maxProj.y, corner.y);
                maxProj.z = glm::max(maxProj.z, corner.z);

                minProj.x = glm::min(minProj.x, corner.x);
                minProj.y = glm::min(minProj.y, corner.y);
                minProj.z = glm::min(minProj.z, corner.z);
            }

            // Tighter frustum for normal meshes
            cascade->frustumMatrix = glm::ortho(minProj.x,
                maxProj.x,
                minProj.y,
                maxProj.y,
                -maxProj.z - 300.0f, // We need to render stuff behind the camera
                -minProj.z + 10.0f) * cascade->viewMatrix; // We need to extend a bit to hide seams at cascade splits

            cascade->terrainFrustumMatrix = glm::ortho(minProj.x,
                maxProj.x,
                minProj.y,
                maxProj.y,
                -maxProj.z - 1250.0f, // We need to render stuff behind the camera
                -minProj.z + 10.0f) * cascade->viewMatrix; // We need to extend a bit to hide seams at cascade splits

            maxLength = glm::ceil(maxLength);

            cascade->projectionMatrix = glm::ortho(-maxLength,
                maxLength,
                -maxLength,
                maxLength,
                -maxLength - 1250.0f, // We need to render stuff behind the camera
                maxLength + 10.0f); // We need to extend a bit to hide seams at cascade splits

            glm::mat4 shadowMatrix = cascade->projectionMatrix * cascade->viewMatrix;
            glm::vec4 shadowOrigin = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            shadowOrigin = shadowMatrix * shadowOrigin;
            shadowOrigin = shadowOrigin * (float)shadow->resolution / 2.0f;

            glm::vec4 roundedOrigin = glm::round(shadowOrigin);
            glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
            roundOffset = roundOffset * 2.0f / (float)shadow->resolution;
            roundOffset.z = 0.0f;
            roundOffset.w = 0.0f;

            const mat4 clip = mat4(1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, -1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.5f, 0.0f,
                0.0f, 0.0f, 0.5f, 1.0f);

            glm::mat4 shadowProj = cascade->projectionMatrix;
            shadowProj[3] += roundOffset;
            cascade->projectionMatrix = clip * shadowProj;
            cascade->frustumMatrix = clip * cascade->frustumMatrix;
            cascade->terrainFrustumMatrix = clip * cascade->terrainFrustumMatrix;

        }

        float DirectionalLight::FrustumSplitFormula(float correction, float nearDist, float farDist, float splitIndex, float splitCount) {

            return correction * nearDist * powf(farDist / nearDist, splitIndex / splitCount) +
                   (1.0f - correction) * (nearDist + (splitIndex / splitCount) * (farDist - nearDist));

        }

    }

}