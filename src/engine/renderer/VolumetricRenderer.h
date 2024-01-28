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
            struct alignas(16) VolumetricUniforms {
                int sampleCount;
                float intensity;
                int fogEnabled;
                float oceanHeight;
                vec4 planetCenterAndRadius;
                Fog fog;
                Light light;
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

            Buffer::UniformBuffer volumetricUniformBuffer;
            Buffer::UniformBuffer blurWeightsUniformBuffer;
            Buffer::UniformBuffer resolveUniformBuffer;

            Ref<Graphics::Sampler> shadowSampler;

        };

    }

}