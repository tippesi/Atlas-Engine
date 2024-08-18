#pragma once

#include "../System.h"

#include "EnvironmentProbe.h"

namespace Atlas {

    namespace Lighting {

        class Atmosphere {
        public:
            Atmosphere(float height = 100000.0f, int32_t probeResolution = 128);

            float height = 100000.0f;

            vec3 rayleighScatteringCoeff = vec3(5.5e-6f, 13.0e-6f, 22.4e-6f);
            float mieScatteringCoeff = 21e-6f;
            float rayleighHeightScale = 8.0e3f;
            float mieHeightScale = 1.2e3f;

            Ref<EnvironmentProbe> probe;

        };

    }

}