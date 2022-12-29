#ifndef AE_INDIRECTLIGHTRENDERER_H
#define AE_INDIRECTLIGHTRENDERER_H

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

	namespace Renderer {

		class IndirectLightRenderer : public Renderer {

		public:
			IndirectLightRenderer();

			void Render(Viewport* viewport, RenderTarget* target,
				Camera* camera, Scene::Scene* scene);

		private:
			OldShader::OldShader shader;

		};

	}

}

#endif