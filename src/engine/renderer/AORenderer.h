#ifndef AE_RTAORENDERER_H
#define AE_RTAORENDERER_H

#include "../System.h"

#include "Renderer.h"
#include "helper/RayTracingHelper.h"

namespace Atlas {

	namespace Renderer {

		class AORenderer : public Renderer {

		public:
			AORenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

			void Render(Viewport* viewport, RenderTarget* target,
				Camera* camera, Scene::Scene* scene) final {};

            void Render(Viewport* viewport, RenderTarget* target, Camera* camera,
                Scene::Scene* scene, Graphics::CommandList* commandList);

		private:
            struct alignas(16) RTUniforms {
                float radius;
                uint32_t frameSeed;
            };

            struct alignas(16) SSUniforms {
                float radius;
                int32_t sampleCount;
                int32_t frameCount;
            };

			Filter blurFilter;
			Helper::RayTracingHelper helper;

			Texture::Texture2D blueNoiseTexture;

            PipelineConfig ssaoPipelineConfig;
            PipelineConfig rtaoPipelineConfig;
            PipelineConfig temporalPipelineConfig;

            PipelineConfig horizontalBlurPipelineConfig;
            PipelineConfig verticalBlurPipelineConfig;

            Buffer::Buffer rtUniformBuffer;
            Buffer::Buffer ssUniformBuffer;
            Buffer::Buffer ssSamplesUniformBuffer;
            Buffer::Buffer blurWeightsUniformBuffer;

		};

	}

}

#endif
