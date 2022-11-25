#ifndef AE_VOLUMETRICRENDERER_H
#define AE_VOLUMETRICRENDERER_H

#include "../System.h"
#include "../Filter.h"
#include "Renderer.h"

namespace Atlas {

	namespace Renderer {

		class VolumetricRenderer : public Renderer {

		public:
			VolumetricRenderer();

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) final;

		private:
			Filter blurFilter;

			Shader::Shader volumetricShader;
			
			Shader::Shader horizontalBlurShader;
			Shader::Shader verticalBlurShader;

			Shader::Shader volumetricResolveShader;

		};

	}

}

#endif