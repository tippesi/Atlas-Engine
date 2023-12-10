#pragma once

#include "../System.h"

namespace Atlas {

    namespace Lighting {

        class Fog {

        public:
            Fog() = default;

            bool enable = true;

            vec3 color = vec3(0.73, 0.79, 0.85) * 0.2f;

            float density = 0.05f;
            float height = 0.0f;
            float heightFalloff = 0.005f;

            float scatteringAnisotropy = -0.6f;

            bool rayMarching = true;
            int32_t rayMarchStepCount = 10;


        };

    }

}