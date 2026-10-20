#pragma once

#include "../System.h"

namespace Atlas {

    namespace Lighting {

        class Fog {

        public:
            Fog() = default;

            bool enable = true;

            float extinctionFactor = 0.16f;
            float scatteringFactor = 2.00f;

            vec4 extinctionCoefficients = vec4(0.93f, 0.965f, 1.0f, 1.0f);

            float density = 0.05f;
            float height = 0.0f;
            float heightFalloff = 0.005f;

            float scatteringAnisotropy = -0.6f;

            float ambientFactor = 0.01f;

            bool rayMarching = true;
            bool localLights = false;
            int32_t rayMarchStepCount = 10;

            bool froxels = true;
            int32_t froxelTileSize = 8;
            int32_t froxelSliceCount = 64;

            // Temporary froxel debug options
            bool froxelDirectionalJitter = true;
            int32_t froxelJitterMode = 0;
            float froxelFarDistance = 0.0f;
            float froxelDistributionExponent = 1.0f;
            bool froxelLocalLightJitter = false;
            int32_t froxelLocalLightSamples = 1;
            float froxelHistoryWeight = 0.9f;

            float volumetricIntensity = 1.0f;

        };

    }

}