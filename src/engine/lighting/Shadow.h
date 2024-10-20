#pragma once

#include "../System.h"
#include "../texture/Cubemap.h"
#include "../texture/Texture2DArray.h"
#include "scene/components/CameraComponent.h"

#define MAX_SHADOW_VIEW_COUNT 5

namespace Atlas {

    namespace Lighting {

        struct ShadowView {

            float nearDistance = 0.0f;
            float farDistance = 100.0f;

            mat4 viewMatrix = mat4(1.0f);
            mat4 projectionMatrix = mat4(1.0f);

            mat4 frustumMatrix = mat4(1.0f);
            mat4 terrainFrustumMatrix = mat4(1.0f);

            vec4 orthoSize = vec4(1.0f);

        };

        class Shadow {

        public:
            Shadow() = default;

            Shadow(float distance, float bias, int32_t resolution, float edgeSoftness, int32_t numCascades, float splitCorrection);

            Shadow(float distance, float bias, int32_t resolution, float edgeSoftness, bool useCubemap = false);

            void SetResolution(int32_t resolution);

            void Update();

            vec3 center = vec3(0.0f);

            float distance = 300.0f;
            float longRangeDistance = 1024.0f;
            float bias = 0.001f;
            float splitCorrection = 0.9f;

            float edgeSoftness = 0.025f;

            float cascadeBlendDistance = 2.5f;

            int32_t resolution = 1024;

            std::vector<ShadowView> views;
            int32_t viewCount = 1;

            Ref<Texture::Texture2DArray> maps;
            Ref<Texture::Cubemap> cubemap;

            bool isCascaded = false;
            bool followMainCamera = false;
            bool useCubemap = false;
            bool allowDynamicEntities = false;
            bool allowTerrain = false;
            bool longRange = false;
            bool update = true;

        };

    }

}