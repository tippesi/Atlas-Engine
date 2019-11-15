#ifndef AE_ATMOSPHERERENDERER_H
#define AE_ATMOSPHERERENDERER_H

#include "../System.h"
#include "Renderer.h"

#include "../lighting/EnvironmentProbe.h"

namespace Atlas {

	namespace Renderer {


		class AtmosphereRenderer : public Renderer {

		public:
			AtmosphereRenderer();

			virtual void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene);

			void Render(Lighting::EnvironmentProbe* probe, Scene::Scene* scene);

			static std::string vertexPath;
			static std::string fragmentPath;

		private:
			void GetUniforms();

			Buffer::VertexArray vertexArray;

			Framebuffer framebuffer;

			Shader::Shader shader;

			Shader::Uniform* viewMatrix = nullptr;
			Shader::Uniform* projectionMatrix = nullptr;
			Shader::Uniform* cameraLocation = nullptr;
			Shader::Uniform* sunDirection = nullptr;
			Shader::Uniform* sunIntensity = nullptr;
			Shader::Uniform* planetCenter = nullptr;
			Shader::Uniform* atmosphereRadius = nullptr;
			Shader::Uniform* planetRadius = nullptr;

		};

	}

}

#endif