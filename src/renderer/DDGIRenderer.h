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

			void DebugProbes(Viewport* viewport, RenderTarget* target,
				Camera* camera, Scene::Scene* scene, 
				std::unordered_map<void*, uint16_t>& materialMap);

			// Used for debugging
			Material probeDebugMaterial;
			Material probeDebugActiveMaterial;
			Material probeDebugInactiveMaterial;

		private:
			Framebuffer irradianceFramebuffer;
			Framebuffer momentsFramebuffer;

			Buffer::Buffer rayHitBuffer;

			Buffer::VertexArray vertexArray;
			Buffer::VertexArray sphereArray;

			Helper::RayTracingHelper helper;

			Shader::Shader probeDebugShader;

			Shader::Shader rayGenShader;
			Shader::Shader rayHitShader;

			Shader::Shader probeStateShader;
			Shader::Shader probeIrradianceUpdateShader;
			Shader::Shader probeMomentsUpdateShader;
			Shader::Shader copyEdgeShader;

		};

	}

}

#endif