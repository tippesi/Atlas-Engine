#ifndef AE_DECALRENDERER_H
#define AE_DECALRENDERER_H

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

	namespace Renderer {

		class DecalRenderer : public Renderer {

		public:
			DecalRenderer();

			void Render(Window* window, RenderTarget* target, Camera* camera, Scene::Scene* scene);

			static std::string vertexPath;
			static std::string fragmentPath;

		private:
			void GetUniforms();

			Buffer::VertexArray vertexArray;

			Shader::Shader shader;

			Shader::Uniform* depthTexture;
			Shader::Uniform* decalTexture;
			Shader::Uniform* modelMatrix;
			Shader::Uniform* viewMatrix;
			Shader::Uniform* projectionMatrix;
			Shader::Uniform* inverseViewMatrix;
			Shader::Uniform* inverseProjectionMatrix;

			Shader::Uniform* color;

			Shader::Uniform* timeInMilliseconds;
			Shader::Uniform* animationLength;
			Shader::Uniform* rowCount;
			Shader::Uniform* columnCount;

		};

	}

}

#endif