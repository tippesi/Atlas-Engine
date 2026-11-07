#pragma once

#include "../System.h"

namespace Atlas {

    namespace Lighting {

        class SSGI {

        public:
            SSGI() = default;

            bool enable = true;
            bool enableAo = true;

            float radius = 1000.0f;
            // Surface thickness per meter of view distance
            float thickness = 0.032f;
            
            int32_t rayCount = 2;
            int32_t sampleCount = 8;

            float irradianceLimit = 10.0f;
            float aoStrength = 1.0f;

            bool halfResolution = true;
            bool ddgi = true;

        };

    }

}