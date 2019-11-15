#include "SkyboxRenderer.h"
#include "helper/GeometryHelper.h"

namespace Atlas {

	namespace Renderer {

		std::string SkyboxRenderer::vertexPath = "skybox.vsh";
		std::string SkyboxRenderer::fragmentPath = "skybox.fsh";

		SkyboxRenderer::SkyboxRenderer() {

			Helper::GeometryHelper::GenerateCubeVertexArray(vertexArray);

			shader.AddStage(AE_VERTEX_STAGE, vertexPath);
			shader.AddStage(AE_FRAGMENT_STAGE, fragmentPath);

			shader.Compile();

			GetUniforms();

		}

		void SkyboxRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

			shader.Bind();

			mat4 mvpMatrix = camera->projectionMatrix * glm::mat4(glm::mat3(camera->viewMatrix));

			modelViewProjectionMatrix->SetValue(mvpMatrix);

			vertexArray.Bind();

			scene->sky.cubemap->Bind(GL_TEXTURE0);

			glDrawArrays(GL_TRIANGLES, 0, 36);

		}

		void SkyboxRenderer::GetUniforms() {

			modelViewProjectionMatrix = shader.GetUniform("mvpMatrix");

		}

	}

}