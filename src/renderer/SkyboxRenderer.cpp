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

			auto lights = scene->GetLights();

			Lighting::DirectionalLight* sun = nullptr;

			for (auto& light : lights) {
				if (light->type == AE_DIRECTIONAL_LIGHT) {
					sun = static_cast<Lighting::DirectionalLight*>(light);
				}
			}

			if (!sun)
				return;

			mvpMatrix->SetValue(matrix);
			ivMatrix->SetValue(camera->invViewMatrix);
			ipMatrix->SetValue(camera->invProjectionMatrix);
			
			cameraLocation->SetValue(camera->GetLocation());
			cameraLocationLast->SetValue(lastCameraLocation);

			lightDirection->SetValue(normalize(sun->direction));
			lightColor->SetValue(sun->color);

			cameraMap[camera] = camera->GetLocation();

			pvMatrixLast->SetValue(camera->GetLastJitteredMatrix());
			jitterLast->SetValue(camera->GetLastJitter());
			jitterCurrent->SetValue(camera->GetJitter());

			if (scene->fog && scene->fog->enable) {

				auto& fog = scene->fog;

				fogScale->SetValue(fog->scale);
				fogDistanceScale->SetValue(fog->distanceScale);
				fogHeight->SetValue(fog->height);
				fogColor->SetValue(fog->color);
				fogScatteringPower->SetValue(fog->scatteringPower);

			}
			else {

				fogScale->SetValue(0.0f);
				fogDistanceScale->SetValue(1.0f);
				fogHeight->SetValue(1.0f);
				fogScatteringPower->SetValue(1.0f);

			}

			vertexArray.Bind();

			scene->sky.cubemap->Bind(GL_TEXTURE0);

			glDrawArrays(GL_TRIANGLES, 0, 36);

		}

		void SkyboxRenderer::GetUniforms() {

			mvpMatrix = shader.GetUniform("mvpMatrix");
			ivMatrix = shader.GetUniform("ivMatrix");
			ipMatrix = shader.GetUniform("ipMatrix");
			cameraLocation = shader.GetUniform("cameraLocation");
			cameraLocationLast = shader.GetUniform("cameraLocationLast");
			lightDirection = shader.GetUniform("lightDirection");
			lightColor = shader.GetUniform("lightColor");

			pvMatrixLast = shader.GetUniform("pvMatrixLast");
			jitterLast = shader.GetUniform("jitterLast");
			jitterCurrent = shader.GetUniform("jitterCurrent");

			fogScale = shader.GetUniform("fogScale");
			fogDistanceScale = shader.GetUniform("fogDistanceScale");
			fogHeight = shader.GetUniform("fogHeight");
			fogColor = shader.GetUniform("fogColor");
			fogScatteringPower = shader.GetUniform("fogScatteringPower");

		}

	}

}