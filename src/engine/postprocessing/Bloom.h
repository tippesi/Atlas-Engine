#pragma once

#include "../System.h"

namespace Atlas::PostProcessing {

    class Bloom {

    public:
        Bloom() = default;

        bool enable = true;
        uint32_t mipLevels = 6;

    };


}