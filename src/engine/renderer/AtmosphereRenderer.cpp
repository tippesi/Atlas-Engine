#include "AtmosphereRenderer.h"
#include "helper/GeometryHelper.h"

#include "../Clock.h"

namespace Atlas {

	namespace Renderer {

		std::string AtmosphereRenderer::vertexPath = "atmosphere.vsh";
		std::string AtmosphereRenderer::fragmentPath = "atmosphere.fsh";

		AtmosphereRenderer::AtmosphereRenderer() {

			Helper::GeometryHelper::GenerateSphereVertexArray(vertexArray, 50, 50);

			shader.AddStage(AE_VERTEX_STAGE, vertexPath);
			shader.AddStage(AE_FRAGMENT_STAGE, fragmentPath);

			shader.Compile();

			GetUniforms();

		}

		void AtmosphereRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

			Profiler::BeginQuery("Atmosphere");

			shader.Bind();

			vertexArray.Bind();

			auto location = camera->GetLocation();

			shader.GetUniform("vMatrix")->SetValue(camera->viewMatrix);
			shader.GetUniform("pMatrix")->SetValue(camera->projectionMatrix);
			shader.GetUniform("cameraLocation")->SetValue(location);
			shader.GetUniform("sunDirection")->SetValue(vec3(0.0f, -1.0f, 0.0f));
			shader.GetUniform("sunIntensity")->SetValue(22.0f);
			shader.GetUniform("planetCenter")->SetValue(-vec3(0.0f, 6371000.0f, 0.0f));
			shader.GetUniform("planetRadius")->SetValue(6371000.0f);
			shader.GetUniform("atmosphereRadius")->SetValue(6471000.0f);
            shader.GetUniform("pvMatrixLast")->SetValue(camera->GetLastJitteredMatrix());
			shader.GetUniform("jitterLast")->SetValue(camera->GetLastJitter());
			shader.GetUniform("jitterCurrent")->SetValue(camera->GetJitter());

			glDrawElements(GL_TRIANGLES, (int32_t)vertexArray.GetIndexComponent()->GetElementCount(),
				vertexArray.GetIndexComponent()->GetDataType(), nullptr);

			Profiler::EndQuery();

		}

		void AtmosphereRenderer::Render(Lighting::EnvironmentProbe* probe, Scene::Scene* scene, Lighting::DirectionalLight* sun) {

			glDisable(GL_DEPTH_TEST);

			shader.Bind();

			vertexArray.Bind();

			cameraLocation->SetValue(probe->GetPosition());
			sunDirection->SetValue(sun->direction);
			sunIntensity->SetValue(22.0f);
			planetCenter->SetValue(-vec3(0.0f, 6371000.0f, 0.0f));
			planetRadius->SetValue(6371000.0f);
			atmosphereRadius->SetValue(6471000.0f);

			viewMatrix->SetValue(mat4(1.0f));

			glViewport(0, 0, probe->cubemap.width, probe->cubemap.height);

			framebuffer.Bind();

			for (uint8_t i = 0; i < 6; i++) {

				projectionMatrix->SetValue(probe->matrices[i]);

				framebuffer.AddComponentCubemap(GL_COLOR_ATTACHMENT0, &probe->cubemap, i);

				glClear(GL_COLOR_BUFFER_BIT);

				glDrawElements(GL_TRIANGLES, (int32_t)vertexArray.GetIndexComponent()->GetElementCount(),
					vertexArray.GetIndexComponent()->GetDataType(), nullptr);

			}

			glEnable(GL_DEPTH_TEST);

			vertexArray.Unbind();
			framebuffer.Unbind();

		}

		void AtmosphereRenderer::GetUniforms() {

			viewMatrix = shader.GetUniform("vMatrix");
			projectionMatrix = shader.GetUniform("pMatrix");
			cameraLocation = shader.GetUniform("cameraLocation");
			sunDirection = shader.GetUniform("sunDirection");
			sunIntensity = shader.GetUniform("sunIntensity");
			planetCenter = shader.GetUniform("planetCenter");
			atmosphereRadius = shader.GetUniform("atmosphereRadius");
			planetRadius = shader.GetUniform("planetRadius");
			pvMatrixLast = shader.GetUniform("pvMatrixLast");
			jitterLast = shader.GetUniform("jitterLast");
			jitterCurrent = shader.GetUniform("jitterCurrent");

		}

	}

}