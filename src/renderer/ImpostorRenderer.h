#ifndef AE_IMPOSTORRENDERER_H
#define AE_IMPOSTORRENDERER_H

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

	namespace Renderer {

		class ImpostorRenderer : public Renderer {

		public:
			ImpostorRenderer();

			virtual void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {}

			virtual void Render(Viewport* viewport, RenderTarget* target, Camera* camera, RenderList* renderList);

			static std::string vertexPath;
			static std::string fragmentPath;

		private:
			void GetUniforms();

			Shader::Shader shader;
			Buffer::VertexArray vertexArray;

			Shader::Uniform* vMatrix = nullptr;
			Shader::Uniform* pMatrix = nullptr;
			Shader::Uniform* cameraLocation = nullptr;

			Shader::Uniform* up = nullptr;
			Shader::Uniform* right = nullptr;

			Shader::Uniform* min = nullptr;
			Shader::Uniform* max = nullptr;

			Shader::Uniform* views = nullptr;

			Shader::Uniform* pvMatrixLast = nullptr;
			Shader::Uniform* jitterCurrent = nullptr;
			Shader::Uniform* jitterLast = nullptr;

		};

	}

}

#endif