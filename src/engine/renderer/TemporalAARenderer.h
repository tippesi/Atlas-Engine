#ifndef AE_TEMPORALAARENDERER_H
#define AE_TEMPORALAARENDERER_H

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

	namespace Renderer {

		class TemporalAARenderer : public Renderer {

		public:
			TemporalAARenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera,
                Scene::Scene* scene, Graphics::CommandList* commandList);

		private:
            struct PushConstants {
                vec2 resolution;
                vec2 invResolution;
                vec2 jitter;
            };

            PipelineConfig pipelineConfig;

			mat4 pvMatrixPrev = mat4(1.0f);

		};

	}

}

#endif