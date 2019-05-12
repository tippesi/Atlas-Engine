#ifndef AE_OCEANRENDERER_H
#define AE_OCEANRENDERER_H

#include "../System.h"
#include "Renderer.h"

#include "gpgpu/OceanSimulation.h"

namespace Atlas {

	namespace Renderer {

		class OceanRenderer : public Renderer {

		public:
			OceanRenderer();

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene);

			void Update();

			static std::string vertexPath;
			static std::string fragmentPath;

			GPGPU::OceanSimulation* simulation;

		private:
			void GetUniforms();

			Buffer::VertexArray vertexArray;

			Texture::Texture2D foam;

			Shader::Shader shader;

			Shader::Uniform* viewMatrix;
			Shader::Uniform* projectionMatrix;

			Shader::Uniform* cameraLocation;

			Shader::Uniform* displacementScale;
			Shader::Uniform* choppyScale;

		};

	}

}


#endif