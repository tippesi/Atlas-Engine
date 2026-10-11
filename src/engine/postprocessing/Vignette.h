#pragma once

#include "../System.h"

namespace Atlas {

    namespace PostProcessing {

        class Vignette {

        public:
            Vignette() = default;

            Vignette(float offset, float power, float strength, vec3 color = vec3(0.0f)) :
                enable(true), offset(offset), power(power), strength(strength), color(color) { };

            bool enable = false;

            float offset = 0.1f;
            float power = 1.0f;
            float strength = 0.1f;

            vec3 color = vec3(0.0f);

        };

    }

}