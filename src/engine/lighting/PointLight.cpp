#include "PointLight.h"

namespace Atlas {

    namespace Lighting {

        PointLight::PointLight(int32_t mobility, float radius) : Light(AE_POINT_LIGHT, mobility), radius(radius) {



        }

        PointLight::~PointLight() {



        }

        void PointLight::AddShadow(float bias, int32_t resolution) {

            if (shadow)
                delete shadow;

            shadow = new Shadow(0.0f, bias, resolution, true);

        }

        void PointLight::RemoveShadow() {

            delete shadow;
            shadow = nullptr;

        }

        void PointLight::AddVolumetric(RenderResolution resolution, int32_t sampleCount, float scattering, float scatteringFactor) {

            volumetric = new Volumetric(resolution, sampleCount);

        }

        void PointLight::RemoveVolumetric() {

            delete volumetric;
            volumetric = nullptr;

        }

        void PointLight::Update(const Scene::Components::CameraComponent& camera) {

            if (mobility == AE_MOVABLE_LIGHT && shadow) {
                shadow->Update();

                mat4 projectionMatrix = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, radius);
                vec3 faces[] = { vec3(1.0f, 0.0f, 0.0f), vec3(-1.0f, 0.0f, 0.0f),
                                 vec3(0.0f, 1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f),
                                 vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 0.0f, -1.0f) };

                vec3 ups[] = { vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f),
                               vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 0.0f, -1.0f),
                               vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f) };

                for (uint8_t i = 0; i < 6; i++) {
                    auto viewMatrix = glm::lookAt(location, location + faces[i], ups[i]);
                    shadow->components[i].projectionMatrix = projectionMatrix;
                    shadow->components[i].viewMatrix = viewMatrix;
                    shadow->components[i].frustumMatrix = projectionMatrix * viewMatrix;
                }
            }

        }

    }

}