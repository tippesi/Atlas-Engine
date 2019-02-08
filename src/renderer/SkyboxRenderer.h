#ifndef AE_SKYBOXRENDERER_H
#define AE_SKYBOXRENDERER_H

#include "../System.h"
#include "IRenderer.h"
#include "buffer/VertexArray.h"

namespace Atlas {

	namespace Renderer {

		class SkyboxRenderer : public IRenderer {

		public:
			SkyboxRenderer();

			virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene);

			static std::string vertexPath;
			static std::string fragmentPath;

		private:
			void GetUniforms();

			Buffer::VertexArray vertexArray;

			Shader::Shader shader;

			Shader::Uniform* skyCubemap;
			Shader::Uniform* modelViewProjectionMatrix;

		};

	}

}

#endif