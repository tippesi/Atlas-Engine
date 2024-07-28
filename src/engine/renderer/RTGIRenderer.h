#pragma once

#include "../System.h"

#include "Renderer.h"
#include "helper/RayTracingHelper.h"

namespace Atlas {

    namespace Renderer {

        class RTGIRenderer : public Renderer {

        public:
            RTGIRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList);

        private:
            struct alignas(16) RTUniforms {
                float radianceLimit;
                uint32_t frameSeed;
                float bias;
                int32_t textureLevel;
                float roughnessCutoff;
                int32_t padding;
                ivec2 resolution;
                Shadow shadow;
            };

            struct alignas(16) TemporalConstants {
                float temporalWeight;
                float historyClipMax;
                float currentClipFactor;
                int32_t resetHistory;
            };

            struct alignas(16) AtrousConstants {
                int32_t stepSize;
                float strength;
            };

            Helper::RayTracingHelper helper;

            Texture::Texture2D scramblingRankingTexture;
            Texture::Texture2D sobolSequenceTexture;

            PipelineConfig rtPipelineConfig;
            PipelineConfig temporalPipelineConfig;
            PipelineConfig atrousPipelineConfig[3];

            Buffer::UniformBuffer rtUniformBuffer;
            Ref<Graphics::Sampler> shadowSampler;
        };

    }

}