#pragma once

#include "../System.h"
#include "Light.h"

namespace Atlas {

    namespace Lighting {

        class PointLight : public Light {

        public:
            PointLight(int32_t mobility = AE_STATIONARY_LIGHT, float radius = 5.0f);

            ~PointLight();

            void AddShadow(float bias, int32_t resolution);

            void RemoveShadow() override;

            void AddVolumetric(RenderResolution resolution, int32_t sampleCount, float scattering, float scatteringFactor = 1.0f);

            void RemoveVolumetric() override;

            void Update(const Scene::Components::CameraComponent& camera) override;

            vec3 location = vec3(0.0f, 3.0f, 0.0f);

            float attenuation = 1.0f;
            float radius = 10.0f;

        };


    }

}