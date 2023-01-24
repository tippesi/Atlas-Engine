#ifndef AE_SSSRENDERER_H
#define AE_SSSRENDERER_H

#include "Renderer.h"

namespace Atlas {

	namespace Renderer {

		class SSSRenderer : public Renderer {

		public:
			SSSRenderer();

			void Render(Viewport* viewport, RenderTarget* target,
				Camera* camera, Scene::Scene* scene) final;

		private:
			Filter blurFilter;
			// Shader::Shader sssShader;

		};


	}

}


#endif