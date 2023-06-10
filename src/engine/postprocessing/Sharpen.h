#ifndef AE_SHARPEN_H
#define AE_SHARPEN_H

#include "../System.h"

namespace Atlas {

    namespace PostProcessing {

        class Sharpen {

        public:
            Sharpen() = default;

            Sharpen(float factor) : enable(true), factor(factor) {}

            bool enable = false;
            float factor = 0.25f;

        };

    }

}

#endif