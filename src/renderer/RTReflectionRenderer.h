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
			Framebuffer framebuffer;
			Buffer::VertexArray vertexArray;

			Filter blurFilter;
			Helper::RayTracingHelper helper;

			Shader::Shader rtrShader;
			
			Shader::Shader horizontalBlurShader;
			Shader::Shader verticalBlurShader;

		};

	}

}

#endif