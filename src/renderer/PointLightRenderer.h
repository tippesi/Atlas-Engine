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

			Shader::Uniform* viewMatrix;
			Shader::Uniform* projectionMatrix;
			Shader::Uniform* inverseProjectionMatrix;
			Shader::Uniform* lightViewMatrix;
			Shader::Uniform* lightProjectionMatrix;
			Shader::Uniform* viewSpaceLightLocation;
			Shader::Uniform* lightLocation;
			Shader::Uniform* lightColor;
			Shader::Uniform* lightAmbient;
			Shader::Uniform* lightRadius;

		};

	}

}

#endif