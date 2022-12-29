#ifndef AE_RTRRENDERER_H
#define AE_RTRRENDERER_H

#include "../System.h"

#include "Renderer.h"
#include "helper/RayTracingHelper.h"

namespace Atlas {

	namespace Renderer {

		class RTReflectionRenderer {

		public:
			RTReflectionRenderer();

			void Render(Viewport* viewport, RenderTarget* target,
				Camera* camera, Scene::Scene* scene);

		private:
			Filter blurFilter;
			Helper::RayTracingHelper helper;

			Texture::Texture2D blueNoiseTexture;

			OldShader::OldShader rtrShader;
			OldShader::OldShader temporalShader;

			OldShader::OldShader atrousShader[3];
		};

	}

}

#endif