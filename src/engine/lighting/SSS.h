#pragma once

#include "../System.h"

namespace Atlas {

    namespace Lighting {

        class SSS {

        public:
            SSS() = default;

            int sampleCount = 8;
            float maxLength = 0.04f;
            float minLengthWorldSpace = 0.5f;
            float thickness = 0.02f;

            bool enable = true;
            bool traceWorldSpace = false;

        };

    }

}