#pragma once

#include "../System.h"
#include "../texture/Cubemap.h"
#include "../texture/Texture2DArray.h"
#include "scene/components/CameraComponent.h"

#define MAX_SHADOW_VIEW_COUNT 5

namespace Atlas {

    namespace Lighting {

        struct ShadowView {

            float nearDistance;
            float farDistance;

            mat4 viewMatrix;
            mat4 projectionMatrix;

            mat4 frustumMatrix;
            mat4 terrainFrustumMatrix;

        };

        class Shadow {

        public:
            Shadow() = default;

            Shadow(float distance, float bias, int32_t resolution, int32_t numCascades, float splitCorrection);

            Shadow(float distance, float bias, int32_t resolution, bool useCubemap = false);

            void SetResolution(int32_t resolution);

            void Update();

            vec3 center = vec3(0.0f);

            float distance = 300.0f;
            float longRangeDistance = 1024.0f;
            float bias = 0.001f;
            float splitCorrection = 0.95f;

            float cascadeBlendDistance = 2.5f;

            int32_t resolution;

            std::vector<ShadowView> views;
            int32_t viewCount;

            Texture::Texture2DArray maps;
            Texture::Cubemap cubemap;

            bool isCascaded = false;
            bool useCubemap = false;
            bool allowDynamicActors = false;
            bool allowTerrain = false;
            bool longRange = false;
            bool update = true;

        };

    }

}