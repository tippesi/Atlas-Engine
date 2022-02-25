#ifndef AE_DIRECTIONALVOLUMETRICRENDERER_H
#define AE_DIRECTIONALVOLUMETRICRENDERER_H

#include "../System.h"
#include "../Filter.h"
#include "Renderer.h"

namespace Atlas {

	namespace Renderer {

		class DirectionalVolumetricRenderer : public Renderer {

		public:
			DirectionalVolumetricRenderer();

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) final;

		private:
			Framebuffer framebuffer;

			Filter blurFilter;

			Shader::Shader volumetricShader;
			Shader::Shader bilateralBlurShader;

		};

	}

}

#endif