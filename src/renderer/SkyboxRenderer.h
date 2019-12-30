#ifndef AE_SKYBOXRENDERER_H
#define AE_SKYBOXRENDERER_H

#include "../System.h"
#include "Renderer.h"
#include "buffer/VertexArray.h"

#include <unordered_map>

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

			std::unordered_map<Camera*, vec3> cameraMap;

			Buffer::VertexArray vertexArray;

			Shader::Shader shader;

			Shader::Uniform* modelViewProjectionMatrix = nullptr;
			Shader::Uniform* cameraLocation = nullptr;
			Shader::Uniform* cameraLocationLast = nullptr;
			Shader::Uniform* pvMatrixLast = nullptr;
			Shader::Uniform* jitterLast = nullptr;
			Shader::Uniform* jitterCurrent = nullptr;

		};

	}

}

#endif