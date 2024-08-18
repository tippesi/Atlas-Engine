#pragma once

#include "../System.h"

namespace Atlas {

    namespace PostProcessing {

        class FilmGrain {

        public:
            FilmGrain() = default;

            FilmGrain(float strength) : enable(true), strength(strength) {};

            bool enable = false;

            float strength = 0.1f;
        };

    }

}