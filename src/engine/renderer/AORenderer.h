#ifndef AE_RTAORENDERER_H
#define AE_RTAORENDERER_H

#include "../System.h"

#include "Renderer.h"
#include "helper/RayTracingHelper.h"

namespace Atlas {

	namespace Renderer {

		class AORenderer {

		public:
			AORenderer();

			void Render(Viewport* viewport, RenderTarget* target,
				Camera* camera, Scene::Scene* scene);

		private:
			Filter blurFilter;
			Helper::RayTracingHelper helper;

			Texture::Texture2D blueNoiseTexture;

			//OldShader::OldShader ssaoShader;
			//OldShader::OldShader rtaoShader;
			//OldShader::OldShader temporalShader;
			
			//OldShader::OldShader horizontalBlurShader;
			//OldShader::OldShader verticalBlurShader;

		};

	}

}

#endif
