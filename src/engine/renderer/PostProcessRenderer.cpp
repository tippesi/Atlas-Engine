#include "PostProcessRenderer.h"

#include "../Clock.h"

namespace Atlas {

	namespace Renderer {

		PostProcessRenderer::PostProcessRenderer() {

            /*
			shader.AddStage(AE_VERTEX_STAGE, "postprocessing.vsh");
			shader.AddStage(AE_FRAGMENT_STAGE, "postprocessing.fsh");

			shader.Compile();

			GetUniforms();

			sharpenShader.AddStage(AE_COMPUTE_STAGE, "sharpen.csh");

			sharpenShader.Compile();

			sharpenFactor = sharpenShader.GetUniform("sharpenFactor");
             */

		}

		void PostProcessRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

            /*
			Profiler::BeginQuery("Postprocessing");

			Profiler::BeginQuery("Main");

			target->postProcessFramebuffer.Bind();

			auto& postProcessing = scene->postProcessing;

			auto& chromaticAberration = postProcessing.chromaticAberration;
			auto& vignette = postProcessing.vignette;
			auto& taa = postProcessing.taa;
			auto& sharpen = postProcessing.sharpen;

			shader.ManageMacro("FILMIC_TONEMAPPING", postProcessing.filmicTonemapping);
			shader.ManageMacro("VIGNETTE", postProcessing.vignette.enable);
			shader.ManageMacro("CHROMATIC_ABERRATION", postProcessing.chromaticAberration.enable);

			shader.Bind();

			ivec2 resolution = ivec2(target->GetWidth(), target->GetHeight());

			hdrTextureResolution->SetValue(vec2(resolution));

			exposure->SetValue(camera->exposure);
			saturation->SetValue(postProcessing.saturation);
			timeInMilliseconds->SetValue(1000.0f * Clock::Get());

			if (chromaticAberration.enable) {
				float reversedValue = chromaticAberration.colorsReversed ? 1.0f : 0.0f;
				aberrationStrength->SetValue(chromaticAberration.strength);
				aberrationReversed->SetValue(reversedValue);
			}

			if (vignette.enable) {
				vignetteOffset->SetValue(vignette.offset);
				vignettePower->SetValue(vignette.power);
				vignetteStrength->SetValue(vignette.strength);
				vignetteColor->SetValue(vignette.color);
			}

			if (taa.enable) {
				target->GetHistory()->Bind(0);
			}
			else {
				target->lightingFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT0)->Bind(0);
			}

			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

			target->postProcessFramebuffer.Unbind();

			Profiler::EndQuery();

			if (sharpen.enable) {
				Profiler::BeginQuery("Sharpen");

				sharpenShader.Bind();

				ivec2 groupCount = resolution / 8;

				groupCount.x += ((groupCount.x * 8 == resolution.x) ? 0 : 1);
				groupCount.y += ((groupCount.y * 8 == resolution.y) ? 0 : 1);

				target->postProcessTexture.Bind(GL_WRITE_ONLY, 0);
				target->postProcessFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT0)->Bind(1);

				sharpenFactor->SetValue(sharpen.factor);

				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

				glDispatchCompute(groupCount.x, groupCount.y, 1);

				Profiler::EndQuery();
			}

			Profiler::EndQuery();
             */

		}

		void PostProcessRenderer::GetUniforms() {

            /*
			hdrTextureResolution = shader.GetUniform("hdrTextureResolution");
			exposure = shader.GetUniform("exposure");
			saturation = shader.GetUniform("saturation");
			bloomPassses = shader.GetUniform("bloomPassses");
			aberrationStrength = shader.GetUniform("aberrationStrength");
			aberrationReversed = shader.GetUniform("aberrationReversed");
			vignetteOffset = shader.GetUniform("vignetteOffset");
			vignettePower = shader.GetUniform("vignettePower");
			vignetteStrength = shader.GetUniform("vignetteStrength");
			vignetteColor = shader.GetUniform("vignetteColor");
			timeInMilliseconds = shader.GetUniform("timeInMilliseconds");
             */

		}

	}

}