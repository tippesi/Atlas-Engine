#include "TemporalAARenderer.h"
#include "helper/GeometryHelper.h"

namespace Atlas {

	namespace Renderer {

		std::string TemporalAARenderer::vertexPath = "taa.vsh";
		std::string TemporalAARenderer::fragmentPath = "taa.fsh";

		TemporalAARenderer::TemporalAARenderer() {

			Helper::GeometryHelper::GenerateRectangleVertexArray(vertexArray);

			shader.AddStage(AE_VERTEX_STAGE, vertexPath);
			shader.AddStage(AE_FRAGMENT_STAGE, fragmentPath);

			shader.Compile();

			GetUniforms();

		}

		void TemporalAARenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

			shader.Bind();
			vertexArray.Bind();

			target->historyFramebuffer.Bind(true);

			target->GetHistory()->Bind(GL_TEXTURE0);
			target->lightingFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT0)->Bind(GL_TEXTURE1);
			target->GetVelocity()->Bind(GL_TEXTURE2);

			jitter->SetValue(camera->GetJitter() * 0.5f);
			invResolution->SetValue(1.0f / vec2((float)target->GetWidth(), (float)target->GetHeight()));
			resolution->SetValue(vec2((float)target->GetWidth(), (float)target->GetHeight()));
			pvMatrixLast->SetValue(camera->GetLastJitteredMatrix());
			ipvMatrixCurrent->SetValue(glm::inverse(camera->projectionMatrix * camera->viewMatrix));

			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

			target->Swap();

		}

		void TemporalAARenderer::GetUniforms() {

			convergence = shader.GetUniform("convergence");
			jitter = shader.GetUniform("jitter");
			invResolution = shader.GetUniform("invResolution");
			resolution = shader.GetUniform("resolution");

			pvMatrixLast = shader.GetUniform("pvMatrixLast");
			ipvMatrixCurrent = shader.GetUniform("ipvMatrixCurrent");

		}

	}

}