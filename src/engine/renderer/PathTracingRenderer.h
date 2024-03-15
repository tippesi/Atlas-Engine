#pragma once

#include "../System.h"
#include "Renderer.h"
#include "helper/RayTracingHelper.h"

#include "../texture/TextureAtlas.h"

#include <unordered_map>

namespace Atlas {

    namespace Renderer {

        class PathTracingRenderer : public Renderer {

        public:
            PathTracingRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Ref<PathTracerRenderTarget> renderTarget, Ref<Scene::Scene> scene,
                ivec2 imageSubdivisions, Graphics::CommandList* commandList);

            bool UpdateData(Scene::Scene* scene);

            void UpdateMaterials(Scene::Scene* scene);

            void ResetSampleCount();

            int32_t GetSampleCount() const;

            int32_t bounces = 10;
            int32_t bvhDepth = 0;
            int32_t lightCount = 512;

            bool realTime = true;
            int32_t realTimeSamplesPerFrame = 4;

            int32_t historyLengthMax = 32;
            float historyClipMax = 0.6f;
            float currentClipFactor = 2.0f;

            float maxRadiance = 65535.0f;

            bool sampleEmissives = false;

        private:
            struct alignas(16) RayGenUniforms {
                vec4 origin;
                vec4 right;
                vec4 bottom;
                ivec2 pixelOffset;
                ivec2 resolution;
                ivec2 tileSize;
                int32_t sampleCount;
            };

            struct alignas(16) RayHitUniforms {
                ivec2 resolution;
                int32_t maxBounces;
                int32_t sampleCount;
                int32_t bounceCount;
                float seed;
                float exposure;
                int32_t samplesPerFrame;
                float maxRadiance;
                int32_t frameCount;
            };

            Helper::RayTracingHelper helper;

            vec3 cameraLocation;
            vec2 cameraRotation;

            int32_t sampleCount = 0;
            ivec2 imageOffset = ivec2(0);

            PipelineConfig rayGenPipelineConfig;
            PipelineConfig rayHitPipelineConfig;

            Buffer::UniformBuffer rayGenUniformBuffer;
            Buffer::UniformBuffer rayHitUniformBuffer;

            size_t frameCount = 0;

        };

    }

}