#ifndef AE_RTAORENDERER_H
#define AE_RTAORENDERER_H

#include "../System.h"

#include "Renderer.h"
#include "helper/RayTracingHelper.h"

namespace Atlas {

	namespace Renderer {

		class RTAORenderer {

		public:
			RTAORenderer();

			void Render(Viewport* viewport, RenderTarget* target,
				Camera* camera, Scene::Scene* scene);

		private:
			Framebuffer framebuffer;
			Buffer::VertexArray vertexArray;

			Filter blurFilter;
			Helper::RayTracingHelper helper;

			Texture::Texture2D blueNoiseTexture;

			Shader::Shader rtaoShader;
			
			Shader::Shader horizontalBlurShader;
			Shader::Shader verticalBlurShader;

		};

	}

}

#endif