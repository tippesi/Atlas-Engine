#pragma once

#include "../System.h"

#include "Renderer.h"
#include "helper/RayTracingHelper.h"

namespace Atlas {

    namespace Renderer {

        class RTReflectionRenderer : public Renderer {

        public:
            RTReflectionRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList);

        private:
            struct alignas(16) RTRUniforms {
                float radianceLimit;
                uint32_t frameSeed;
                float bias;
                int32_t textureLevel;
                Shadow shadow;
            };

            struct alignas(16) TemporalConstants {
                float temporalWeight;
                float historyClipMax;
                float currentClipFactor;
            };

            struct alignas(16) AtrousConstants {
                int32_t stepSize;
                float strength;
            };

            Helper::RayTracingHelper helper;

            Texture::Texture2D scramblingRankingTexture;
            Texture::Texture2D sobolSequenceTexture;

            PipelineConfig rtrPipelineConfig;
            PipelineConfig temporalPipelineConfig;

            PipelineConfig atrousPipelineConfig[3];

            Buffer::UniformBuffer rtrUniformBuffer;
            Ref<Graphics::Sampler> shadowSampler;
        };

    }

}