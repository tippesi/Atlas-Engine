#pragma once

#include "../System.h"
#include "../texture/Texture3D.h"
#include "../texture/Texture2D.h"

#include "scene/components/CameraComponent.h"

namespace Atlas {

    namespace Lighting {

        class VolumetricClouds {

        public:
            VolumetricClouds() = default;

            VolumetricClouds(int32_t coverageResolution, int32_t shapeResolution,
                int32_t detailResolution, int32_t shadowResolution = 1024);

            void GetShadowMatrices(const CameraComponent& camera, vec3 lightDirection, mat4& viewMatrix, mat4& projectionMatrix);

            void SetShadowResolution(int32_t shadowResolution);

            void GenerateCoverageTexture();

            Texture::Texture2D coverageTexture;
            Texture::Texture3D shapeTexture;
            Texture::Texture3D detailTexture;
            Texture::Texture2D shadowTexture;

            struct Scattering {
                float extinctionFactor = 0.16f;
                float scatteringFactor = 1.25f;

                vec4 extinctionCoefficients = vec4(0.93f, 0.965f, 1.0f, 1.0f);

                // Henyey-Greenstein eccentricities, positive values scatter forward (silver lining)
                float eccentricityFirstPhase = 0.0f;
                float eccentricitySecondPhase = 0.5f;
                float phaseAlpha = 0.5f;
            };

            // Placement of the clouds in the weather map, from no clouds (0) to overcast (1)
            float coverage = 0.40f;
            // Blend from stratus (0) over stratocumulus (0.5) to cumulus (1)
            float cloudType = 0.68f;
            // Approximate horizontal size of a single cloud in meters
            float cloudSize = 1000.0f;
            // From flat (0) to towering (1) clouds
            float verticalDevelopment = 0.5f;
            // Strength of the high frequency erosion at the cloud edges
            float detail = 0.64f;
            // Optical mass of the formed clouds
            float densityMultiplier = 1.0f;

            float baseAltitude = 1400.0f;
            float layerThickness = 1800.0f;
            float distanceLimit = 8000.0f;

            // Set needsNoiseUpdate after changing the seed
            float variationSeed = 4.23f;

            // Low (0), medium (1), high (2) and ultra (3)
            int32_t quality = 1;

            Scattering scattering;

            bool needsNoiseUpdate = true;
            bool enable = true;
            bool castShadow = true;

        };

    }

}
