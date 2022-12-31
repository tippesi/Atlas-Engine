#ifndef AE_POSTPROCESSRENDERER_H
#define AE_POSTPROCESSRENDERER_H

#include "../System.h"
#include "Renderer.h"
#include "shader/OldShader.h"

namespace Atlas {

	namespace Renderer {

		class PostProcessRenderer : public Renderer {

		public:
			PostProcessRenderer() = default;

            void Init(GraphicsDevice* device);

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) final {}

            void Render(Viewport* viewport, RenderTarget* target, Camera* camera,
                Scene::Scene* scene, CommandList* commandList);

		private:
            struct alignas(16) Uniforms {
                float exposure;
                float saturation;
                float timeInMilliseconds;
                int32_t bloomPasses;
                float aberrationStrength;
                float aberrationReversed;
                float vignetteOffset;
                float vignettePower;
                float vignetteStrength;
                vec4 vignetteColor;
            };

			void SetUniforms(Camera* camera, Scene::Scene* scene);

            PipelineConfig GetMainPipelineConfig();

            PipelineConfig GetMainPipelineConfig(const Ref<FrameBuffer> frameBuffer);

            PipelineConfig mainPipelineSwapChainConfig;
            PipelineConfig mainPipelineFrameBufferConfig;
            PipelineConfig sharpenPipelineConfig;

            Ref<MultiBuffer> uniformBuffer;

		};

	}

}

#endif