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

		}

		void AtmosphereRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

			Profiler::BeginQuery("Atmosphere");

			shader.Bind();

			vertexArray.Bind();

			auto sun = scene->sky.sun;
			auto atmosphere = scene->sky.atmosphere;
			if (!sun || !atmosphere) return;

			auto location = camera->GetLocation();

			shader.GetUniform("vMatrix")->SetValue(camera->viewMatrix);
			shader.GetUniform("pMatrix")->SetValue(camera->projectionMatrix);
			shader.GetUniform("cameraLocation")->SetValue(location);
			shader.GetUniform("sunDirection")->SetValue(sun->direction);
			shader.GetUniform("sunIntensity")->SetValue(sun->intensity);
			shader.GetUniform("planetCenter")->SetValue(scene->sky.planetCenter);
			shader.GetUniform("planetRadius")->SetValue(scene->sky.planetRadius);
			shader.GetUniform("atmosphereRadius")->SetValue(scene->sky.planetRadius + atmosphere->height);
            shader.GetUniform("pvMatrixLast")->SetValue(camera->GetLastJitteredMatrix());
			shader.GetUniform("jitterLast")->SetValue(camera->GetLastJitter());
			shader.GetUniform("jitterCurrent")->SetValue(camera->GetJitter());

			glDrawElements(GL_TRIANGLES, (int32_t)vertexArray.GetIndexComponent()->GetElementCount(),
				vertexArray.GetIndexComponent()->GetDataType(), nullptr);

			Profiler::EndQuery();

		}

		void AtmosphereRenderer::Render(Lighting::EnvironmentProbe* probe, Scene::Scene* scene) {

			Profiler::BeginQuery("Atmosphere environment probe");

			glDisable(GL_DEPTH_TEST);

			shader.Bind();

			vertexArray.Bind();

			auto sun = scene->sky.sun;
			auto atmosphere = scene->sky.atmosphere;
			if (!sun || !atmosphere) return;

			shader.GetUniform("vMatrix")->SetValue(mat4(1.0f));
			shader.GetUniform("cameraLocation")->SetValue(probe->GetPosition());
			shader.GetUniform("sunDirection")->SetValue(sun->direction);
			shader.GetUniform("sunIntensity")->SetValue(sun->intensity);
			shader.GetUniform("planetCenter")->SetValue(scene->sky.planetCenter);
			shader.GetUniform("planetRadius")->SetValue(scene->sky.planetRadius);
			shader.GetUniform("atmosphereRadius")->SetValue(scene->sky.planetRadius + atmosphere->height);
			shader.GetUniform("pvMatrixLast")->SetValue(mat4(1.0f));
			shader.GetUniform("jitterLast")->SetValue(vec2(1.0f));
			shader.GetUniform("jitterCurrent")->SetValue(vec2(1.0f));

			glViewport(0, 0, probe->cubemap.width, probe->cubemap.height);

			framebuffer.Bind();

			Profiler::BeginQuery("Render probe faces");

			for (uint8_t i = 0; i < 6; i++) {

				shader.GetUniform("pMatrix")->SetValue(probe->matrices[i]);

				framebuffer.AddComponentCubemap(GL_COLOR_ATTACHMENT0, &probe->cubemap, i);

				glClear(GL_COLOR_BUFFER_BIT);

				glDrawElements(GL_TRIANGLES, (int32_t)vertexArray.GetIndexComponent()->GetElementCount(),
					vertexArray.GetIndexComponent()->GetDataType(), nullptr);

			}

			glEnable(GL_DEPTH_TEST);

			vertexArray.Unbind();
			framebuffer.Unbind();

			Profiler::EndAndBeginQuery("Generate mipmaps");

			probe->cubemap.GenerateMipmap();

			Profiler::EndQuery();
			Profiler::EndQuery();

		}

	}

}