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

            bool localLights = false;

            int32_t froxelTileSize = 8;
            int32_t froxelSliceCount = 64;
            bool froxelFilter = false;
            // End of the regular slices, 0 uses the camera far plane. The last slice reaches up to the far plane
            float froxelFarDistance = 1000.0f;
            // Below 1 moves slices from the near plane to the distance, the grid always starts at the near plane
            float froxelDistributionExponent = 0.7f;

            float volumetricIntensity = 1.0f;

        };

    }

}