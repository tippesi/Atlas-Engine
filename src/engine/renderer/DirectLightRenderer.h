#ifndef AE_DIRECTLIGHTRENDERER_H
#define AE_DIRECTLIGHTRENDERER_H

#include "Renderer.h"

namespace Atlas {

	namespace Renderer {

		class DirectLightRenderer : public Renderer {

		public:
			DirectLightRenderer() = default;

			void Init(GraphicsDevice* device);

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) final {}

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera,
				Scene::Scene* scene, CommandList* commandList);

		private:


		};

	}

}

#endif