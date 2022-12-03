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

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) final;

		private:
			void GetUniforms();

			std::unordered_map<Camera*, vec3> cameraMap;

			Buffer::VertexArray vertexArray;

			Shader::Shader shader;

			Shader::Uniform* mvpMatrix = nullptr;
			Shader::Uniform* ivMatrix = nullptr;
			Shader::Uniform* ipMatrix = nullptr;
			Shader::Uniform* cameraLocation = nullptr;
			Shader::Uniform* cameraLocationLast = nullptr;

			Shader::Uniform* pvMatrixLast = nullptr;
			Shader::Uniform* jitterLast = nullptr;
			Shader::Uniform* jitterCurrent = nullptr;

			Shader::Uniform* fogScale = nullptr;
			Shader::Uniform* fogDistanceScale = nullptr;
			Shader::Uniform* fogHeight = nullptr;
			Shader::Uniform* fogColor = nullptr;
			Shader::Uniform* fogScatteringPower = nullptr;

		};

	}

}

#endif