#pragma once

#include "../../System.h"

#include <vector>

namespace Atlas {

    namespace Renderer {

        namespace Helper {

            class HaltonSequence {

            public:
                static std::vector<float> Generate(int32_t base, int32_t count);

                static std::vector<vec2> Generate(int32_t baseX, int32_t baseY, int32_t count);

            private:
                static float Halton(int32_t base, int32_t index);

            };

        }

    }

}