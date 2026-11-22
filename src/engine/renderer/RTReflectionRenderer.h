#pragma once

#include "../System.h"

#include "Renderer.h"
#include "helper/RayTracingHelper.h"
#include "helper/DeferredShadowRays.h"

namespace Atlas {

    namespace Renderer {

        class RTReflectionRenderer : public Renderer {

        public:
            RTReflectionRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            // The G-buffer channels the reflections start from where the ocean or the front layer of the translucent
            // surfaces is closer, see RenderTarget::SetSeparateReflectionData
            void RenderReflectionData(Ref<RenderTarget> target, Ref<Scene::Scene> scene, bool ocean,
                const RenderTargetData* frontLayer, Graphics::CommandList* commandList);

            void Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList);

        private:
            // State the passes of a frame share. The upsampling swaps the two reflection textures
            struct ReflectionFrame {
                Ref<Lighting::Reflection> reflection;
                ivec2 rayRes;
                ivec2 res;
                Texture::Texture2D* reflectionTexture;
                Texture::Texture2D* swapReflectionTexture;
                bool upsample;
                bool radianceVolume;
                uint32_t frameCount;
            };

            void RenderSSR(const ReflectionFrame& frame, Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList);

            // Ray traced reflections where SSR found no hit, with deferred shadow rays without hardware ray tracing
            void TraceRays(const ReflectionFrame& frame, Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList);

            void Upsample(ReflectionFrame& frame, Ref<RenderTarget> target, Graphics::CommandList* commandList);

            void TemporalFilter(const ReflectionFrame& frame, Ref<RenderTarget> target, Graphics::CommandList* commandList);

            void SpatialFilter(const ReflectionFrame& frame, Ref<RenderTarget> target, Graphics::CommandList* commandList);

            struct alignas(16) RTRUniforms {
                float radianceLimit;
                uint32_t frameSeed;
                float bias;
                int32_t sampleCount;
                int32_t lightSampleCount;
                int32_t textureLevel;
                float roughnessCutoff;
                int32_t halfRes;
                ivec2 resolution;
                uint32_t frameCount;
                int32_t hiZLevelCount;
            };

            struct alignas(16) ResolveConstants {
                ivec2 resolution;
                int32_t sampleCount;
                int32_t lightSampleCount;
            };

            struct alignas(16) UpscalingConstants {
                uint32_t frameCount;
            };

            struct alignas(16) TemporalConstants {
                float temporalWeight;
                float historyClipMax;
                float currentClipFactor;
                float roughnessCutoff;
                int32_t resetHistory;
            };

            struct alignas(16) AtrousConstants {
                float strength;
                float roughnessCutoff;
            };

            Helper::RayTracingHelper helper;

            Texture::Texture2D scramblingRankingTexture;
            Texture::Texture2D sobolSequenceTexture;

            PipelineConfig ssrPipelineConfig;
            PipelineConfig rtrPipelineConfig;
            PipelineConfig rtrRayTracingPipelineConfig;
            PipelineConfig rtrResolvePipelineConfig;
            PipelineConfig upsamplePipelineConfig;
            PipelineConfig temporalPipelineConfig;
            PipelineConfig reflectionDataPipelineConfig;

            Ref<Graphics::Sampler> nearestSampler;
            PipelineConfig atrousPipelineConfig[3];

            Buffer::UniformBuffer rtrUniformBuffer;

            Helper::DeferredShadowRays shadowRays;
        };

    }

}