#pragma once

#include "../System.h"

namespace Atlas {

    namespace PostProcessing {

        class TAA {

        public:
            TAA() = default;

            TAA(float jitterRange) : jitterRange(jitterRange) {}

            float jitterRange = 0.99f;

        };

    }

}