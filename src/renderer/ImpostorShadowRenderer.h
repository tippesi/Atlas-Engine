#ifndef AE_IMPOSTORSHADOWRENDERER_H
#define AE_IMPOSTORSHADOWRENDERER_H

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

	namespace Renderer {

		class ImpostorShadowRenderer : public Renderer {

		public:
			ImpostorShadowRenderer();

			virtual void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {}

			virtual void Render(Viewport* viewport, RenderTarget* target, RenderList* renderList,
				mat4 viewMatrix, mat4 projectionMatrix, vec3 location);

		private:
			void GetUniforms();

			Shader::Shader shader;
			Buffer::VertexArray vertexArray;

			Shader::Uniform* vMatrix = nullptr;
			Shader::Uniform* pMatrix = nullptr;
			Shader::Uniform* cameraLocation = nullptr;

			Shader::Uniform* center = nullptr;
			Shader::Uniform* radius = nullptr;

			Shader::Uniform* views = nullptr;
			Shader::Uniform* cutoff = nullptr;

		};

	}

}

#endif