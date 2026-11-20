#pragma once

#include "../System.h"
#include "../buffer/Buffer.h"
#include "../graphics/Buffer.h"

namespace Atlas::PostProcessing {

    class AutoExposure {

    public:
        AutoExposure();

        bool enable = true;

        float luminanceMin = 0.001f;
        // Above sunlit snow
        float luminanceMax = 100000.0f;
        float blackLevel = 0.001f;

        float timeCoefficient = 1.0f;

        Buffer::Buffer histogramBuffer;
        // The average luminance the post processing measured, copied for the pre-exposure of later frames
        Ref<Graphics::MultiBuffer> luminanceReadback;

        // Average scene luminance of a frame a few frames ago, 0 before the first measurement
        float GetMeasuredLuminance() const;

    };
    
}