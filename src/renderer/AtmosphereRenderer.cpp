#include "AtmosphereRenderer.h"
#include "helper/GeometryHelper.h"

#include "../Clock.h"

namespace Atlas {

	namespace Renderer {

		std::string AtmosphereRenderer::vertexPath = "atmosphere.vsh";
		std::string AtmosphereRenderer::fragmentPath = "atmosphere.fsh";

		AtmosphereRenderer::AtmosphereRenderer() {

			Helper::GeometryHelper::GenerateSphereVertexArray(vertexArray, 200, 200);

			shader.AddStage(AE_VERTEX_STAGE, vertexPath);
			shader.AddStage(AE_FRAGMENT_STAGE, fragmentPath);

			shader.Compile();

			GetUniforms();

		}

		void AtmosphereRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

			shader.Bind();

			vertexArray.Bind();

			auto location = camera->thirdPerson ? camera->location -
				camera->direction * camera->thirdPersonDistance : camera->location;

			viewMatrix->SetValue(camera->viewMatrix);
			projectionMatrix->SetValue(camera->projectionMatrix);
			cameraLocation->SetValue(location);
			sunDirection->SetValue(vec3(0.0f, -1.0f, 0.0f));
			sunIntensity->SetValue(22.0f);
			planetCenter->SetValue(-vec3(0.0f, 6371000.0f, 0.0f));
			planetRadius->SetValue(6371000.0f);
			atmosphereRadius->SetValue(6471000.0f);

			glDrawElements(GL_TRIANGLES, (int32_t)vertexArray.GetIndexComponent()->GetElementCount(),
				vertexArray.GetIndexComponent()->GetDataType(), nullptr);

		}

		void AtmosphereRenderer::Render(Lighting::EnvironmentProbe* probe, Scene::Scene* scene) {

			glDisable(GL_DEPTH_TEST);

			shader.Bind();

			vertexArray.Bind();

			cameraLocation->SetValue(probe->GetPosition());
			sunDirection->SetValue(vec3(0.0f, -1.0f, 0.0f));
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

		}

	}

}