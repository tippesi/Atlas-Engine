#include "BVHRenderer.h"

namespace Atlas {

	namespace Renderer {

		std::string BVHRenderer::vertexPath = "debug/bvhDebug.vsh";
		std::string BVHRenderer::fragmentPath = "debug/bvhDebug.fsh";

		BVHRenderer::BVHRenderer() {

			indexBuffer = Buffer::IndexBuffer(AE_UINT, sizeof(uint32_t), 
				1, AE_BUFFER_DYNAMIC_STORAGE);
			vertexArray.AddIndexComponent(&indexBuffer);
			vertexBuffer = Buffer::VertexBuffer(AE_FLOAT, 3, 
				sizeof(vec3), 8, AE_BUFFER_DYNAMIC_STORAGE);
			vertexArray.AddComponent(0, &vertexBuffer);

			shader.AddStage(AE_VERTEX_STAGE, vertexPath);
			shader.AddStage(AE_FRAGMENT_STAGE, fragmentPath);

			shader.Compile();

			GetUniforms();

		}

		void BVHRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

			return;

		}

		void BVHRenderer::GetUniforms() {

			viewMatrix = shader.GetUniform("vMatrix");
			projectionMatrix = shader.GetUniform("pMatrix");
			modelMatrix = shader.GetUniform("mMatrix");

			color = shader.GetUniform("color");

		}

	}

}