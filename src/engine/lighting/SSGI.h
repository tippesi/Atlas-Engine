#pragma once

#include "../System.h"

namespace Atlas {

    namespace Lighting {

        class SSGI {

        public:
            SSGI() = default;

            bool enable = true;

            float radius = 5.0f;
            uint32_t sampleCount = 16;

            bool rt = false;
            bool opacityCheck = false;

        };

    }

}