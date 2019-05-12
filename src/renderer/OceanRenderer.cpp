#include "OceanRenderer.h"
#include "helper/GeometryHelper.h"

namespace Atlas {

	namespace Renderer {

		std::string OceanRenderer::vertexPath = "ocean/ocean.vsh";
		std::string OceanRenderer::fragmentPath = "ocean/ocean.fsh";

		OceanRenderer::OceanRenderer() {

			Helper::GeometryHelper::GenerateGridVertexArray(vertexArray, 512, 0.25f);

			simulation = new GPGPU::OceanSimulation(128, 2000);

			foam = Texture::Texture2D("foam.jpg", false);

			shader.AddStage(AE_VERTEX_STAGE, vertexPath);
			shader.AddStage(AE_FRAGMENT_STAGE, fragmentPath);

			shader.Compile();

			GetUniforms();

		}

		void OceanRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

			shader.Bind();

			vertexArray.Bind();

			displacementScale->SetValue(0.75f);
			choppyScale->SetValue(0.75f);
			cameraLocation->SetValue(vec3(camera->viewMatrix[3]));

			simulation->displacementMap.Bind(GL_TEXTURE0);
			simulation->normalMap.Bind(GL_TEXTURE1);

			if (scene->sky.skybox != nullptr)
				scene->sky.skybox->cubemap->Bind(GL_TEXTURE3);

			foam.Bind(GL_TEXTURE2);

			viewMatrix->SetValue(camera->viewMatrix);
			projectionMatrix->SetValue(camera->projectionMatrix);

			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
				GL_SHADER_STORAGE_BARRIER_BIT);

			glDrawElements(GL_TRIANGLE_STRIP, (int32_t)vertexArray.GetIndexComponent()->GetElementCount(),
				vertexArray.GetIndexComponent()->GetDataType(), nullptr);

		}

		void OceanRenderer::Update() {

			simulation->Compute();

		}

		void OceanRenderer::GetUniforms() {

			viewMatrix = shader.GetUniform("vMatrix");
			projectionMatrix = shader.GetUniform("pMatrix");

			cameraLocation = shader.GetUniform("cameraLocation");

			displacementScale = shader.GetUniform("displacementScale");
			choppyScale = shader.GetUniform("choppyScale");

		}

	}

}