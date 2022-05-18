#include "TemporalAARenderer.h"
#include "helper/GeometryHelper.h"

namespace Atlas {

	namespace Renderer {

		TemporalAARenderer::TemporalAARenderer() {

			shader.AddStage(AE_COMPUTE_STAGE, "taa.csh");
			shader.Compile();

		}

		void TemporalAARenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

			Profiler::BeginQuery("TAA");

			shader.Bind();

			auto res = ivec2(target->GetWidth(), target->GetHeight());

			const int32_t groupSize = 8;
			ivec2 groupCount = res / groupSize;
			groupCount.x += ((res.x % groupSize == 0) ? 0 : 1);
			groupCount.y += ((res.y % groupSize == 0) ? 0 : 1);

			target->GetLastHistory()->Bind(GL_WRITE_ONLY, 0);

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

			glDispatchCompute(groupCount.x, groupCount.y, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			target->Swap();

			Profiler::EndQuery();

		}

	}

}