#ifndef AE_SKYBOXRENDERER_H
#define AE_SKYBOXRENDERER_H

#include "../System.h"
#include "Renderer.h"
#include "buffer/VertexArray.h"

namespace Atlas {

	namespace Renderer {

		class SkyboxRenderer : public Renderer {

		public:
			SkyboxRenderer();

			virtual void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene);

			static std::string vertexPath;
			static std::string fragmentPath;

		private:
			void GetUniforms();

			Buffer::VertexArray vertexArray;

			Shader::Shader shader;

			Shader::Uniform* modelViewProjectionMatrix = nullptr;

		};

	}

}

#endif