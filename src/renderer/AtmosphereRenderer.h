#ifndef AE_ATMOSPHERERENDERER_H
#define AE_ATMOSPHERERENDERER_H

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

	namespace Renderer {


		class AtmosphereRenderer : public Renderer {

		public:
			AtmosphereRenderer();

			virtual void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene);

			static std::string vertexPath;
			static std::string fragmentPath;

		private:
			void GetUniforms();

			Buffer::VertexArray vertexArray;

			Shader::Shader shader;

			Shader::Uniform* viewMatrix;
			Shader::Uniform* projectionMatrix;
			Shader::Uniform* cameraLocation;
			Shader::Uniform* sunDirection;
			Shader::Uniform* sunIntensity;
			Shader::Uniform* planetCenter;
			Shader::Uniform* atmosphereRadius;
			Shader::Uniform* planetRadius;

		};

	}

}

#endif