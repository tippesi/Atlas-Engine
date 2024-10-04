#pragma once

#include "../System.h"
#include "../Filter.h"
#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        class VolumetricRenderer : public Renderer {

        public:
            VolumetricRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList);

        private:
            struct alignas(16) CullingPushConstants {
                int32_t lightCount;
            };

            struct alignas(16) VolumetricUniforms {
                int sampleCount;
                float intensity;
                int fogEnabled;
                float oceanHeight;
                int lightCount;
                int offsetX;
                int offsetY;
                int directionalLightCount;
                vec4 planetCenterAndRadius;
                Fog fog;
                CloudShadow cloudShadow;
            };

            struct alignas(16) BlurConstants {

            };

            struct alignas(16) ResolveUniforms {
                Fog fog;
                vec4 planetCenter;
                int downsampled2x;
                int cloudsEnabled;
                int fogEnabled;
                float innerCloudRadius;
                float planetRadius;
                float cloudDistanceLimit;
            };

            Filter blurFilter;

            PipelineConfig volumetricPipelineConfig;

            PipelineConfig horizontalBlurPipelineConfig;
            PipelineConfig verticalBlurPipelineConfig;

            PipelineConfig resolvePipelineConfig;

            Texture::Texture2D scramblingRankingTexture;
            Texture::Texture2D sobolSequenceTexture;

            Buffer::Buffer lightCullingBuffer;
            Buffer::UniformBuffer volumetricUniformBuffer;
            Buffer::UniformBuffer blurWeightsUniformBuffer;
            Buffer::UniformBuffer resolveUniformBuffer;
            
            Ref<Graphics::Sampler> shadowSampler;

        };

    }

}