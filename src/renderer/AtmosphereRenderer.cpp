#include "AtmosphereRenderer.h"
#include "helper/GeometryHelper.h"

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

		void AtmosphereRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene) {

			shader.Bind();

			vertexArray.Bind();

			viewMatrix->SetValue(camera->viewMatrix);
			projectionMatrix->SetValue(camera->projectionMatrix);
			cameraLocation->SetValue(vec3(camera->location));
			sunDirection->SetValue(vec3(0.0f, -0.1f, -1.0f));

			glDrawElements(GL_TRIANGLES, vertexArray.GetIndexComponent()->GetElementCount(),
						   vertexArray.GetIndexComponent()->GetDataType(), NULL);

		}

		void AtmosphereRenderer::GetUniforms() {

			viewMatrix = shader.GetUniform("vMatrix");
			projectionMatrix = shader.GetUniform("pMatrix");
			cameraLocation = shader.GetUniform("cameraLocation");
			sunDirection = shader.GetUniform("sunDirection");

		}

	}

}