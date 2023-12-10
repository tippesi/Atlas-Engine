#pragma once

#include "../System.h"

namespace Atlas {

    namespace PostProcessing {

        class ChromaticAberration {

        public:
            ChromaticAberration() = default;

            ///
            /// \param strength
            /// \param colorsReversed
            ChromaticAberration(float strength, bool colorsReversed = false) :
                    strength(strength), colorsReversed(colorsReversed), enable(true) {};

            bool enable = false;

            float strength = 1.0f;
            bool colorsReversed = false;

        };

    }

}