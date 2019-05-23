#ifndef AE_POINTLIGHTRENDERER_H
#define AE_POINTLIGHTRENDERER_H

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

	namespace Renderer {

		class PointLightRenderer : public Renderer {

		public:
			PointLightRenderer();

			virtual void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene);

			static std::string vertexPath;
			static std::string fragmentPath;

		private:
			void GetUniforms();

			Buffer::VertexArray vertexArray;

			Shader::Shader shader;

			Shader::Uniform* viewMatrix = nullptr;
			Shader::Uniform* projectionMatrix = nullptr;
			Shader::Uniform* inverseProjectionMatrix = nullptr;
			Shader::Uniform* lightViewMatrix = nullptr;
			Shader::Uniform* lightProjectionMatrix = nullptr;
			Shader::Uniform* viewSpaceLightLocation = nullptr;
			Shader::Uniform* lightLocation = nullptr;
			Shader::Uniform* lightColor = nullptr;
			Shader::Uniform* lightAmbient = nullptr;
			Shader::Uniform* lightRadius = nullptr;

		};

	}

}

#endif