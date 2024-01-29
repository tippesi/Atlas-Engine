#pragma once

#include "../System.h"

namespace Atlas {

    namespace Lighting {

        class SSGI {

        public:
            SSGI() = default;

            bool enable = true;
            bool enableAo = true;

            float radius = 1.0f;
            
            int32_t rayCount = 2;
            int32_t sampleCount = 8;

            float irradianceLimit = 10.0f;
            float aoStrength = 1.0f;

            bool rt = false;
            bool opacityCheck = false;

        };

    }

}