#ifndef IRRADIANCEVOLUMERENDERER_H
#define IRRADIANCEVOLUMERENDERER_H

#include "../System.h"
#include "Renderer.h"
#include "helper/RayTracingHelper.h"

namespace Atlas {

	namespace Renderer {

		class DDGIRenderer {

		public:
			DDGIRenderer();

			void Render(Viewport* viewport, RenderTarget* target,
				Camera* camera, Scene::Scene* scene);

			void TraceAndUpdateProbes(Scene::Scene* scene);

		private:
			Framebuffer irradianceFramebuffer;
			Framebuffer momentsFramebuffer;

			Buffer::Buffer rayHitBuffer;

			Buffer::VertexArray vertexArray;

			Helper::RayTracingHelper helper;

			Shader::Shader rayGenShader;
			Shader::Shader rayHitShader;

			Shader::Shader probeStateShader;
			Shader::Shader probeUpdateShader;
			Shader::Shader copyEdgeShader;

		};

	}

}

#endif