#ifndef AE_POINTLIGHTRENDERER_H
#define AE_POINTLIGHTRENDERER_H

#include "../System.h"
#include "IRenderer.h"

namespace Atlas {

	namespace Renderer {

		class PointLightRenderer : public IRenderer {

		public:
			PointLightRenderer();

			virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene);

			static std::string vertexPath;
			static std::string fragmentPath;

		private:
			void GetUniforms();

			Buffer::VertexArray vertexArray;

			Shader::Shader shader;

			Shader::Uniform* diffuseTexture;
			Shader::Uniform* normalTexture;
			Shader::Uniform* materialTexture;
			Shader::Uniform* depthTexture;
			Shader::Uniform* shadowCubemap;

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