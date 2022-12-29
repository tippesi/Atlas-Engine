#ifndef AE_IMPOSTORSHADOWRENDERER_H
#define AE_IMPOSTORSHADOWRENDERER_H

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

	namespace Renderer {

		class ImpostorShadowRenderer : public Renderer {

		public:
			ImpostorShadowRenderer();

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) final {}

			void Render(Viewport* viewport, RenderTarget* target, RenderList* renderList,
				mat4 viewMatrix, mat4 projectionMatrix, vec3 location);

		private:
			void GetUniforms();

			OldShader::OldShader shader;
			OldBuffer::VertexArray vertexArray;

			OldShader::Uniform* vMatrix = nullptr;
			OldShader::Uniform* pMatrix = nullptr;
			OldShader::Uniform* cameraLocation = nullptr;

			OldShader::Uniform* center = nullptr;
			OldShader::Uniform* radius = nullptr;

			OldShader::Uniform* views = nullptr;
			OldShader::Uniform* cutoff = nullptr;

		};

	}

}

#endif