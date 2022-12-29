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

			OldBuffer::VertexArray vertexArray;

			OldShader::OldShader shader;

			OldShader::Uniform* mvpMatrix = nullptr;
			OldShader::Uniform* ivMatrix = nullptr;
			OldShader::Uniform* ipMatrix = nullptr;
			OldShader::Uniform* cameraLocation = nullptr;
			OldShader::Uniform* cameraLocationLast = nullptr;

			OldShader::Uniform* pvMatrixLast = nullptr;
			OldShader::Uniform* jitterLast = nullptr;
			OldShader::Uniform* jitterCurrent = nullptr;

			OldShader::Uniform* fogScale = nullptr;
			OldShader::Uniform* fogDistanceScale = nullptr;
			OldShader::Uniform* fogHeight = nullptr;
			OldShader::Uniform* fogColor = nullptr;
			OldShader::Uniform* fogScatteringPower = nullptr;

		};

	}

}

#endif