#ifndef AE_VOLUMETRICRENDERER_H
#define AE_VOLUMETRICRENDERER_H

#include "../System.h"
#include "../Filter.h"
#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        class VolumetricRenderer : public Renderer {

        public:
            VolumetricRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Viewport* viewport, RenderTarget* target, Camera* camera,
                Scene::Scene* scene, Graphics::CommandList* commandList);

        private:
            struct alignas(16) Fog {
                float density;
                float heightFalloff;
                float height;
                float scatteringAnisotropy;
                vec4 color;
            };

            struct alignas(16) VolumetricUniforms {
                int sampleCount;
                float intensity;
                float seed;
                int fogEnabled;
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

#endif