#pragma once

#include "../System.h"
#include "../texture/Texture2D.h"

namespace Atlas {

    namespace Lighting {

        class RTGI {

        public:
            RTGI() = default;

            int32_t textureLevel = 4;
            int32_t lightSampleCount = 2;

            float radianceLimit = 5.0f;
            float bias = 0.15f;
            float spatialFilterStrength = 5.0f;

            float temporalWeight = 0.97f;
            float historyClipMax = 1.0f;
            float currentClipFactor = 2.0f;

            bool enable = true;
            bool ddgi = true;
            bool useShadowMap = false;
            bool useNormalMaps = true;
            bool opacityCheck = false;
            bool halfResolution = true;

        };

    }

}