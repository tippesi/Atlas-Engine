#pragma once

#include "../System.h"
#include "../texture/Texture2D.h"

namespace Atlas {

    namespace Lighting {

        class Reflection {

        public:
            Reflection() = default;

            int32_t textureLevel = 3;
            float radianceLimit = 10.0f;
            float roughnessCutoff = 0.9f;
            float bias = 0.15f;
            float spatialFilterStrength = 5.0f;

            float temporalWeight = 0.95f;
            float historyClipMax = 1.0f;
            float currentClipFactor = 2.0f;

            bool enable = true;
            bool rt = true;
            bool gi = true;
            bool useShadowMap = false;
            bool useNormalMaps = true;
            bool opacityCheck = false;
            bool halfResolution = true;

        };

    }

}