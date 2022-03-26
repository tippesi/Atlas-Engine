#include "IndirectLightRenderer.h"

namespace Atlas {

	namespace Renderer {

		IndirectLightRenderer::IndirectLightRenderer() {

			shader.AddStage(AE_COMPUTE_STAGE, "deferred/indirect.csh");
			shader.Compile();

		}

		void IndirectLightRenderer::Render(Viewport* viewport, RenderTarget* target,
			Camera* camera, Scene::Scene* scene) {

			Profiler::BeginQuery("Indirect lighting");

			shader.Bind();
			
			auto volume = scene->irradianceVolume;
			if (volume && volume->enable) {
				auto [irradianceArray, momentsArray] = volume->internal.GetCurrentProbes();
				irradianceArray.Bind(GL_TEXTURE12);
				momentsArray.Bind(GL_TEXTURE13);
				volume->internal.probeStateBuffer.BindBase(1);
				shader.GetUniform("volumeEnabled")->SetValue(true);
				shader.GetUniform("volumeMin")->SetValue(volume->aabb.min);
				shader.GetUniform("volumeMax")->SetValue(volume->aabb.max);
				shader.GetUniform("volumeProbeCount")->SetValue(volume->probeCount);
				shader.GetUniform("volumeIrradianceRes")->SetValue(volume->irrRes);
				shader.GetUniform("volumeMomentsRes")->SetValue(volume->momRes);
				shader.GetUniform("volumeBias")->SetValue(volume->bias);
				shader.GetUniform("volumeGamma")->SetValue(volume->gamma);
				shader.GetUniform("cellSize")->SetValue(volume->cellSize);
				shader.GetUniform("indirectStrength")->SetValue(volume->strength);
			}
			else {
				shader.GetUniform("volumeEnabled")->SetValue(false);
				shader.GetUniform("volumeMin")->SetValue(vec3(0.0f));
				shader.GetUniform("volumeMax")->SetValue(vec3(0.0f));
			}

			auto ssao = scene->ssao;
			target->ssaoTexture.Bind(GL_TEXTURE6);
			if (ssao && ssao->enable) shader.GetUniform("aoEnabled")->SetValue(true);
			else shader.GetUniform("aoEnabled")->SetValue(false);

			shader.GetUniform("ivMatrix")->SetValue(camera->invViewMatrix);
			shader.GetUniform("ipMatrix")->SetValue(camera->invProjectionMatrix);

			target->lightingFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT0)->Bind(GL_READ_WRITE, 0);

			auto resolution = ivec2(target->GetWidth(), target->GetHeight());
			auto groupCount = resolution / 8;

			groupCount.x += ((groupCount.x * 8 == resolution.x) ? 0 : 1);
			groupCount.y += ((groupCount.y * 8 == resolution.y) ? 0 : 1);

			glDispatchCompute(groupCount.x, groupCount.y, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			Profiler::EndQuery();

		}

	}

}
