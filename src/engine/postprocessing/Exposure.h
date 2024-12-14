#pragma once

#include "../System.h"
#include "../buffer/Buffer.h"

namespace Atlas::PostProcessing {

    class Exposure {

    public:
        Exposure();

        bool autoExposure = true;
        float luminanceMin = 0.0f;
        float luminanceMax = 100.0f;

        float timeCoefficient = 0.1f;

        Buffer::Buffer histogramBuffer;

    };
    
}