#ifndef AE_DECALRENDERER_H
#define AE_DECALRENDERER_H

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

	namespace Renderer {

		class DecalRenderer : public Renderer {

		public:
			DecalRenderer();

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene);

			static std::string vertexPath;
			static std::string fragmentPath;

		private:
			void GetUniforms();

			Buffer::VertexArray vertexArray;

			Shader::Shader shader;

			Shader::Uniform* modelMatrix = nullptr;
			Shader::Uniform* viewMatrix = nullptr;
			Shader::Uniform* projectionMatrix = nullptr;
			Shader::Uniform* inverseViewMatrix = nullptr;
			Shader::Uniform* inverseProjectionMatrix = nullptr;

			Shader::Uniform* color = nullptr;

			Shader::Uniform* timeInMilliseconds = nullptr;
			Shader::Uniform* animationLength = nullptr;
			Shader::Uniform* rowCount = nullptr;
			Shader::Uniform* columnCount = nullptr;

		};

	}

}

#endif