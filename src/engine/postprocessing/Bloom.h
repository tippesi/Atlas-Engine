#pragma once

#include "../System.h"

namespace Atlas::PostProcessing {

    class Bloom {

    public:
        Bloom() = default;

        bool enable = true;
        float strength = 0.01f;
        float threshold = 0.5f;

        float filterSize = 0.02f;
        uint32_t mipLevels = 6;

    };


}