#include "SSSRenderer.h"

namespace Atlas {

	namespace Renderer {

		SSSRenderer::SSSRenderer() {
            /*
			sssShader.AddStage(AE_COMPUTE_STAGE, "deferred/sss.csh");
			sssShader.Compile();
            */
		}

		void SSSRenderer::Render(Viewport* viewport, RenderTarget* target,
			Camera* camera, Scene::Scene* scene) {

            /*
			auto sss = scene->sss;
			auto sun = scene->sky.sun;

			if (!sss || !sss->enable || !sun) return;

			Profiler::BeginQuery("Screen space shadows");

			ivec2 res = ivec2(target->sssTexture.width, target->sssTexture.height);
			auto downsampledTarget = target->GetDownsampledTextures(RenderResolution::FULL_RES);

			auto depthTexture = downsampledTarget->depthTexture;
			auto normalTexture = downsampledTarget->geometryNormalTexture;

			{
				ivec2 groupCount = ivec2(res.x / 8, res.y / 8);
				groupCount.x += ((groupCount.x * 8 == res.x) ? 0 : 1);
				groupCount.y += ((groupCount.y * 8 == res.y) ? 0 : 1);

				static int frameCount = 0;

				auto lightDirection = glm::normalize(vec3(camera->viewMatrix * vec4(sun->direction, 0.0f)));

				sssShader.Bind();

				depthTexture->Bind(0);
				normalTexture->Bind(1);

				target->sssTexture.Bind(GL_WRITE_ONLY, 0);

				sssShader.GetUniform("vMatrix")->SetValue(camera->viewMatrix);
				sssShader.GetUniform("pMatrix")->SetValue(camera->projectionMatrix);
				sssShader.GetUniform("ipMatrix")->SetValue(camera->invProjectionMatrix);
				sssShader.GetUniform("ivMatrix")->SetValue(camera->invViewMatrix);
				sssShader.GetUniform("lightDirection")->SetValue(lightDirection);
				sssShader.GetUniform("frameCount")->SetValue(frameCount++);

				sssShader.GetUniform("sampleCount")->SetValue(sss->sampleCount);
				sssShader.GetUniform("maxLength")->SetValue(sss->maxLength);
				sssShader.GetUniform("thickness")->SetValue(sss->thickness);

				glMemoryBarrier(GL_ALL_BARRIER_BITS);
				glDispatchCompute(groupCount.x, groupCount.y, 1);
				glMemoryBarrier(GL_ALL_BARRIER_BITS);

			}

			Profiler::EndQuery();
            */

		}

	}

}