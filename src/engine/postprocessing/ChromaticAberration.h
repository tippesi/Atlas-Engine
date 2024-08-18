#pragma once

#include "../System.h"

namespace Atlas {

    namespace PostProcessing {

        class ChromaticAberration {

        public:
            ChromaticAberration() = default;

            ChromaticAberration(float strength, bool colorsReversed = false) :
                enable(true), strength(strength), colorsReversed(colorsReversed) {};

            bool enable = false;

            float strength = 1.0f;
            bool colorsReversed = false;

        };

    }

}