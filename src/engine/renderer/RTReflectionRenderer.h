#ifndef AE_RTRRENDERER_H
#define AE_RTRRENDERER_H

#include "../System.h"

#include "Renderer.h"
#include "helper/RayTracingHelper.h"

namespace Atlas {

	namespace Renderer {

		class RTReflectionRenderer {

		public:
			RTReflectionRenderer() = default;

			void Init(Graphics::GraphicsDevice* device);

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

            struct alignas(16) RTRUniforms {
                float radianceLimit;
                uint32_t frameSeed;
                float bias;
                float padding;
                Shadow shadow;
            };

            struct alignas(16) AtrousConstants {
                int32_t stepSize;
                float strength;
            };

			Helper::RayTracingHelper helper;

			Texture::Texture2D blueNoiseTexture;

			PipelineConfig rtrPipelineConfig;
			PipelineConfig temporalPipelineConfig;

			PipelineConfig atrousPipelineConfig[3];

            Buffer::Buffer rtrUniformBuffer;
            Ref<Graphics::Sampler> shadowSampler;
		};

	}

}

#endif