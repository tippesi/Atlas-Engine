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

			OldShader::OldShader volumetricShader;
			
			OldShader::OldShader horizontalBlurShader;
			OldShader::OldShader verticalBlurShader;

			OldShader::OldShader volumetricResolveShader;

		};

	}

}

#endif