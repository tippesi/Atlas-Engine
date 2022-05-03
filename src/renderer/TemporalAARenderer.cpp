#include "TemporalAARenderer.h"
#include "helper/GeometryHelper.h"

namespace Atlas {

	namespace Renderer {

		TemporalAARenderer::TemporalAARenderer() {

			Helper::GeometryHelper::GenerateRectangleVertexArray(vertexArray);

			shader.AddStage(AE_VERTEX_STAGE, "taa.vsh");
			shader.AddStage(AE_FRAGMENT_STAGE, "taa.fsh");

			shader.Compile();

		}

		void TemporalAARenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

			Profiler::BeginQuery("TAA");

			shader.Bind();
			vertexArray.Bind();

			target->historyFramebuffer.Bind(true);

			target->GetHistory()->Bind(GL_TEXTURE0);
			target->lightingFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT0)->Bind(GL_TEXTURE1);
			target->GetVelocity()->Bind(GL_TEXTURE2);
			target->lightingFramebuffer.GetComponentTexture(GL_DEPTH_ATTACHMENT)->Bind(GL_TEXTURE3);
			target->GetLastVelocity()->Bind(GL_TEXTURE4);
			target->lightingFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT2)->Bind(GL_TEXTURE5);

			shader.GetUniform("invResolution")->SetValue(1.0f / vec2((float)target->GetWidth(), (float)target->GetHeight()));
			shader.GetUniform("resolution")->SetValue(vec2((float)target->GetWidth(), (float)target->GetHeight()));
			shader.GetUniform("pvMatrixLast")->SetValue(camera->GetLastJitteredMatrix());
			shader.GetUniform("ipvMatrixCurrent")->SetValue(glm::inverse(camera->projectionMatrix * camera->viewMatrix));
			shader.GetUniform("jitter")->SetValue(camera->GetJitter());

			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

			target->Swap();

			Profiler::EndQuery();

		}

	}

}