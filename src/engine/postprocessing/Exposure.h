#pragma once

#include "../System.h"
#include "../buffer/Buffer.h"

namespace Atlas::PostProcessing {

    class AutoExposure {

    public:
        AutoExposure();

        bool enable = true;

        float luminanceMin = 0.001f;
        float luminanceMax = 1000.0f;
        float blackLevel = 0.001f;

        float timeCoefficient = 1.0f;

        Buffer::Buffer histogramBuffer;

    };
    
}