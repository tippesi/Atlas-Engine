#pragma once

#include "../System.h"

namespace Atlas {

    namespace PostProcessing {

        class TAA {

        public:
            TAA() = default;

            TAA(float jitterRange) : enable(true), jitterRange(jitterRange) {}

            bool enable = false;

            float jitterRange = 0.99f;

        };

    }

}