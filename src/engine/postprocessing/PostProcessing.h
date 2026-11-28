#pragma once

#include "../System.h"

#include "Vignette.h"
#include "ChromaticAberration.h"
#include "Sharpen.h"
#include "FilmGrain.h"
#include "TAA.h"
#include "Bloom.h"
#include "Exposure.h"

namespace Atlas {

    namespace PostProcessing {

        // Persisted as integers, append new upscalers at the end
        enum class Upscaler {
            None = 0,
            TAA,
            FSR2,
            FSR4,
            DLSS
        };

        class PostProcessing {

        public:
            // In the order of the enum, the FSR4 upscaler runs FSR 3 on GPUs without support for FSR 4
            static constexpr const char* upscalerNames[] = { "None", "TAA", "FSR 2", "FSR 3/4", "DLSS" };

            // Persisted values aren't restricted while loading
            static const char* GetUpscalerName(Upscaler upscaler) {
                auto index = size_t(upscaler);
                return index < std::size(upscalerNames) ? upscalerNames[index] : "Unknown";
            }

            float exposureCompensation = 0.0f;
            float saturation = 1.0f;
            float contrast = 1.0f;
            vec3 tint = vec3(1.0f);

            float screenMaxLuminance = 1000.0f;
            float paperWhiteLuminance = 150.0f;

            bool filmicTonemapping = false;

            Upscaler upscaler = Upscaler::FSR2;

            TAA taa;
            Vignette vignette;
            ChromaticAberration chromaticAberration;
            FilmGrain filmGrain;
            Sharpen sharpen;
            Bloom bloom;
            AutoExposure autoExposure;

        };

    }

}
