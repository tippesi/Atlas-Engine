#pragma once

#include "Renderer.h"
#include "helper/RayTracingHelper.h"

namespace Atlas {

    namespace Renderer {

        class SSGIRenderer : public Renderer {

        public:
            SSGIRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList);

        private:
            Graphics::GraphicsDevice* device;

            struct alignas(16) SSUniforms {
                float radianceLimit;
                uint32_t frameSeed;
                float radius;
                uint32_t rayCount;
                uint32_t sampleCount;
                int32_t downsampled2x;
            };

            Filter blurFilter;

            Texture::Texture2D scramblingRankingTexture;
            Texture::Texture2D sobolSequenceTexture;

            PipelineConfig ssgiPipelineConfig;
            PipelineConfig temporalPipelineConfig;

            PipelineConfig horizontalBlurPipelineConfig;
            PipelineConfig verticalBlurPipelineConfig;

            Buffer::UniformBuffer ssUniformBuffer;
            Buffer::UniformBuffer blurWeightsUniformBuffer;

        };

    }

}