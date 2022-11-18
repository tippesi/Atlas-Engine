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
				irradianceArray.Bind(GL_TEXTURE24);
				momentsArray.Bind(GL_TEXTURE25);
				volume->internal.probeStateBuffer.BindBase(14);
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

			auto ao = scene->ao;
			auto reflection = scene->reflection;

			target->aoTexture.Bind(GL_TEXTURE6);
			target->GetDownsampledTextures(target->GetAOResolution())->depthTexture->Bind(GL_TEXTURE14);
			target->reflectionTexture.Bind(GL_TEXTURE16);
			shader.GetUniform("aoEnabled")->SetValue(ao && ao->enable);
			shader.GetUniform("aoDownsampled2x")->SetValue(target->GetAOResolution() == RenderResolution::HALF_RES);
			shader.GetUniform("reflectionEnabled")->SetValue(reflection && reflection->enable);
			shader.GetUniform("aoStrength")->SetValue(ao && ao->enable ? ao->strength : 1.0f);

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
