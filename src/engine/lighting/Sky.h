#pragma once

#include "../System.h"

#include "EnvironmentProbe.h"
#include "Atmosphere.h"
#include "VolumetricClouds.h"

namespace Atlas {

    namespace Lighting {

        class Sky {

        public:
            Sky() = default;

            Ref<EnvironmentProbe> GetProbe();

            // The factor the atmosphere applies to a directional light reaching a position (Atmosphere::directLight),
            // one without an atmosphere
            vec3 GetDirectLightTransmittance(vec3 position, vec3 lightDirection) const;

            vec3 planetCenter = vec3(0.0f, -650000.0f, 0.0f);
            float planetRadius = 649000.0f;

            float intensity = 1.0f;

            Ref<Atmosphere> atmosphere = CreateRef<Atmosphere>();
            Ref<VolumetricClouds> clouds = nullptr;

            Ref<EnvironmentProbe> probe = nullptr;

        };

    }

}