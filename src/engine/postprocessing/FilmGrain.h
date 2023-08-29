#ifndef AE_FILMGRAIN_H
#define AE_FILMGRAIN_H

#include "../System.h"

namespace Atlas {

    namespace PostProcessing {

        class FilmGrain {

        public:
            FilmGrain() = default;

            FilmGrain(float strength) : strength(strength), enable(true) {};

            bool enable = false;

            float strength = 0.1f;
        };

    }

}


#endif