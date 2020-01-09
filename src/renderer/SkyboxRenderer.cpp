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

			mat4 mvpMatrix = camera->projectionMatrix * camera->viewMatrix;

			vec3 lastCameraLocation = vec3(0.0f);
			auto& key = cameraMap.find(camera);
			if (key != cameraMap.end()) {
				lastCameraLocation = key->second;
			}
			else {
				cameraMap[camera] = camera->GetLocation();
				key = cameraMap.find(camera);
			}

			modelViewProjectionMatrix->SetValue(mvpMatrix);
			cameraLocation->SetValue(camera->GetLocation());
			cameraLocationLast->SetValue(lastCameraLocation);

			key->second = camera->GetLocation();

			pvMatrixLast->SetValue(camera->GetLastJitteredMatrix());
			jitterLast->SetValue(camera->GetLastJitter());
			jitterCurrent->SetValue(camera->GetJitter());

			vertexArray.Bind();

			scene->sky.cubemap->Bind(GL_TEXTURE0);

			glDrawArrays(GL_TRIANGLES, 0, 36);

		}

		void SkyboxRenderer::GetUniforms() {

			modelViewProjectionMatrix = shader.GetUniform("mvpMatrix");
			cameraLocation = shader.GetUniform("cameraLocation");
			cameraLocationLast = shader.GetUniform("cameraLocationLast");
			pvMatrixLast = shader.GetUniform("pvMatrixLast");
			jitterLast = shader.GetUniform("jitterLast");
			jitterCurrent = shader.GetUniform("jitterCurrent");

		}

	}

}