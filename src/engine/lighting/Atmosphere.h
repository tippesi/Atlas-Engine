#pragma once

#include "../System.h"

#include "EnvironmentProbe.h"
#include "../texture/Texture2D.h"

namespace Atlas {

    namespace Lighting {

        class Atmosphere {
        public:
            Atmosphere(float height = 100000.0f, int32_t probeResolution = 128);

            float height = 100000.0f;

            vec3 rayleighScatteringCoeff = vec3(5.5e-6f, 13.0e-6f, 22.4e-6f);
            vec3 groundAlbedo = vec3(0.03f);
            float mieScatteringCoeff = 21e-6f;
            float rayleighHeightScale = 8.0e3f;
            float mieHeightScale = 1.2e3f;
            // Angular diameter of the sun disk in degrees
            float sunDiskSize = 0.535f;
            // How much the air between the camera and the sun colors and dims the direct light of the main light,
            // from not at all (0) to the full transmittance (1): red and weak near the horizon
            float directLight = 0.0f;
            // Scales how much the sky lights the scene indirectly (the probe of the atmosphere: ambient light,
            // global illumination and reflections of the sky), the visible sky stays as it is
            float indirectLight = 1.0f;
            bool needsUpdate = true;

            Ref<EnvironmentProbe> probe;
            Texture::Texture2D transmittanceLutTexture;
            Texture::Texture2D multipleScatteringLutTexture;
            Texture::Texture2D skyViewLutTexture;

        };

    }

}
