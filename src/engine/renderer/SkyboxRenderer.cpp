#include "SkyboxRenderer.h"
#include "helper/GeometryHelper.h"

namespace Atlas {

	namespace Renderer {

		SkyboxRenderer::SkyboxRenderer() {

            /*
			Helper::GeometryHelper::GenerateCubeVertexArray(vertexArray);

			shader.AddStage(AE_VERTEX_STAGE, "skybox.vsh");
			shader.AddStage(AE_FRAGMENT_STAGE, "skybox.fsh");

			shader.Compile();

			GetUniforms();
             */

		}

		void SkyboxRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

            /*
			Profiler::BeginQuery("Skybox");

			shader.Bind();

			mat4 matrix = camera->projectionMatrix * camera->viewMatrix;

			vec3 lastCameraLocation = vec3(0.0f);
			auto key = cameraMap.find(camera);
			if (key != cameraMap.end()) {
				lastCameraLocation = key->second;
			}
			else {
				cameraMap[camera] = camera->GetLocation();
				key = cameraMap.find(camera);
			}

			mvpMatrix->SetValue(matrix);
			ivMatrix->SetValue(camera->invViewMatrix);
			ipMatrix->SetValue(camera->invProjectionMatrix);
			
			cameraLocation->SetValue(camera->GetLocation());
			cameraLocationLast->SetValue(lastCameraLocation);

			cameraMap[camera] = camera->GetLocation();

			pvMatrixLast->SetValue(camera->GetLastJitteredMatrix());
			jitterLast->SetValue(camera->GetLastJitter());
			jitterCurrent->SetValue(camera->GetJitter());

			vertexArray.Bind();

			scene->sky.probe->cubemap.Bind(0);

			glDrawArrays(GL_TRIANGLES, 0, 36);

			Profiler::EndQuery();
             */

		}

		void SkyboxRenderer::GetUniforms() {

            /*
			mvpMatrix = shader.GetUniform("mvpMatrix");
			ivMatrix = shader.GetUniform("ivMatrix");
			ipMatrix = shader.GetUniform("ipMatrix");
			cameraLocation = shader.GetUniform("cameraLocation");
			cameraLocationLast = shader.GetUniform("cameraLocationLast");

			pvMatrixLast = shader.GetUniform("pvMatrixLast");
			jitterLast = shader.GetUniform("jitterLast");
			jitterCurrent = shader.GetUniform("jitterCurrent");

			fogScale = shader.GetUniform("fogScale");
			fogDistanceScale = shader.GetUniform("fogDistanceScale");
			fogHeight = shader.GetUniform("fogHeight");
			fogColor = shader.GetUniform("fogColor");
			fogScatteringPower = shader.GetUniform("fogScatteringPower");
             */

		}

	}

}