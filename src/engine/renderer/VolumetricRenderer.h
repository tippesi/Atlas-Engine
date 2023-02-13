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

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) final {};

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera,
                Scene::Scene* scene, Graphics::CommandList* commandList);

		private:
            struct alignas(16) Cascade {
                float distance;
                float texelSize;
                float aligment0;
                float aligment1;
                mat4 cascadeSpace;
            };

            struct alignas(16) Shadow {
                float distance;
                float bias;

                float cascadeBlendDistance;

                int cascadeCount;
                vec2 resolution;

                Cascade cascades[6];
            };

            struct alignas(16) Light {
                vec4 location;
                vec4 direction;

                vec4 color;
                float intensity;

                float scatteringFactor;

                float radius;

                Shadow shadow;
            };

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
            };

            struct alignas(16) BlurConstants {

            };

            struct alignas(16) ResolveUniforms {
                Fog fog;
                int downsampled2x;
                int cloudsEnabled;
                int fogEnabled;
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