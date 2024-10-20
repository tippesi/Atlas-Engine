#pragma once

#include "../System.h"

namespace Atlas {

    namespace Lighting {

        class Fog {

        public:
            Fog() = default;

            bool enable = true;

            float extinctionFactor = 0.16f;
            float scatteringFactor = 2.00f;

            vec4 extinctionCoefficients = vec4(0.93f, 0.965f, 1.0f, 1.0f);

            float density = 0.05f;
            float height = 0.0f;
            float heightFalloff = 0.005f;

            float scatteringAnisotropy = -0.6f;

            float ambientFactor = 0.01f;

            bool rayMarching = true;
            bool localLights = false;
            int32_t rayMarchStepCount = 10;

            float volumetricIntensity = 1.0f;

        };

    }

}