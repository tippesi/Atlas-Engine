#pragma once

#include "../System.h"

namespace Atlas {

    namespace Lighting {

        class SSGI {

        public:
            SSGI() = default;

            bool enable = true;
            bool ao = true;

            float radius = 1.0f;
            
            int32_t rayCount = 4;
            int32_t sampleCount = 8;

            float irradianceLimit = 5.0f;

            bool rt = false;
            bool opacityCheck = false;

        };

    }

}