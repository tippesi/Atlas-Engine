#pragma once

#include "../System.h"
#include "../Camera.h"
#include "../RenderTarget.h"
#include "../texture/Texture3D.h"

namespace Atlas {

    namespace Lighting {

        class VolumetricClouds {

        public:
            VolumetricClouds(int32_t coverageResolution = 512, int32_t shapeResolution = 128,
                int32_t detailResolution = 32, int32_t shadowResolution = 512);

            void GetShadowMatrices(Camera* camera, vec3 lightDirection, mat4& viewMatrix, mat4& projectionMatrix);

            Texture::Texture2D coverageTexture;
            Texture::Texture3D shapeTexture;
            Texture::Texture3D detailTexture;
            Texture::Texture2D shadowTexture;

            struct Scattering {
                float extinctionFactor = 0.24f;
                float scatteringFactor = 2.00f;

                vec4 extinctionCoefficients = vec4(0.93f, 0.965f, 1.0f, 1.0f);

                float eccentricityFirstPhase = 0.0f;
                float eccentricitySecondPhase = -0.5f;
                float phaseAlpha = 0.5f;
            };

            int32_t sampleCount = 64;
            int32_t occlusionSampleCount = 5;
            int32_t shadowSampleFraction = 4;

            float minHeight = 100.0f;
            float maxHeight = 600.0f;
            float distanceLimit = 8000.0f;

            float coverageScale = 0.25f;
            float shapeScale = 2.0f;
            float detailScale = 16.0f;
            float coverageSpeed = 5.0f;
            float shapeSpeed = 5.0f;
            float detailSpeed = 10.0f;
            float detailStrength = 0.15f;

            float densityMultiplier = 0.8f;

            float heightStretch = 0.5f;

            float darkEdgeFocus = 2.0f;
            float darkEdgeAmbient = 0.1f;

            Scattering scattering;

            bool needsNoiseUpdate = true;
            bool enable = true;
            bool castShadow = true;

        };

    }

}