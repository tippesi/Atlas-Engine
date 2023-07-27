#ifndef AE_REFLECTION_H
#define AE_REFLECTION_H

#include "../System.h"
#include "../RenderTarget.h"
#include "../texture/Texture2D.h"

namespace Atlas {

    namespace Lighting {

        class Reflection {

        public:
            Reflection(int32_t sampleCount = 1);

            void SetSampleCount(int32_t sampleCount);

            int32_t sampleCount = 1;
            int32_t textureLevel = 3;
            float radianceLimit = 10.0f;
            float bias = 0.15f;
            float spatialFilterStrength = 5.0f;

            float temporalWeight = 0.95f;
            float historyClipMax = 0.1f;
            float currentClipFactor = 2.0f;

            bool enable = true;
            bool rt = false;
            bool gi = true;
            bool useShadowMap = false;

        };

    }

}


#endif