#ifndef AE_SSAORENDERER_H
#define AE_SSAORENDERER_H

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

	namespace Renderer {

		class SSAORenderer : public Renderer {

		public:
			SSAORenderer();

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) final;

		private:
			Framebuffer framebuffer;
			Filter blurFilter;

			Shader::Shader ssaoShader;
			Shader::Shader bilateralBlurShader;

		};

	}

}

#endif