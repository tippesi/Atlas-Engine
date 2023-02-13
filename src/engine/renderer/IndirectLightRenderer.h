#ifndef AE_INDIRECTLIGHTRENDERER_H
#define AE_INDIRECTLIGHTRENDERER_H

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

	namespace Renderer {

		class IndirectLightRenderer : public Renderer {

		public:
			IndirectLightRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

			void Render(Viewport* viewport, RenderTarget* target,
				Camera* camera, Scene::Scene* scene) final {};

            void Render(Viewport* viewport, RenderTarget* target, Camera* camera,
                Scene::Scene* scene, Graphics::CommandList* commandList);

		private:
            struct Uniforms {
                int aoEnabled;
                int aoDownsampled2x;
                int reflectionEnabled;
                float aoStrength;
            };

			PipelineConfig pipelineConfig;

            Buffer::UniformBuffer uniformBuffer;

		};

	}

}

#endif