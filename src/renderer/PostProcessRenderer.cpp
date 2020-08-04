#include "PostProcessRenderer.h"
#include "MasterRenderer.h"

#include "../Clock.h"

namespace Atlas {

	namespace Renderer {

		PostProcessRenderer::PostProcessRenderer() {

			shader.AddStage(AE_VERTEX_STAGE, "postprocessing.vsh");
			shader.AddStage(AE_FRAGMENT_STAGE, "postprocessing.fsh");

			shader.Compile();

			GetUniforms();

			sharpenShader.AddStage(AE_COMPUTE_STAGE, "sharpen.csh");

			sharpenShader.Compile();

			sharpenFactor = sharpenShader.GetUniform("sharpenFactor");

		}

		void PostProcessRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

			target->postProcessFramebuffer.Bind();

			auto postProcessing = &scene->postProcessing;

			bool hasFilmicTonemappingMacro = shader.HasMacro("FILMIC_TONEMAPPING");
			bool hasVignetteMacro = shader.HasMacro("VIGNETTE");
			bool hasChromaticAberrationMacro = shader.HasMacro("CHROMATIC_ABERRATION");

			if (postProcessing->filmicTonemapping && !hasFilmicTonemappingMacro) {
				shader.AddMacro("FILMIC_TONEMAPPING");
			}
			else if (!postProcessing->filmicTonemapping && hasFilmicTonemappingMacro) {
				shader.RemoveMacro("FILMIC_TONEMAPPING");
			}

			if (postProcessing->vignette && !hasVignetteMacro) {
				shader.AddMacro("VIGNETTE");
			}
			else if (!postProcessing->vignette && hasVignetteMacro) {
				shader.RemoveMacro("VIGNETTE");
			}

			if (postProcessing->chromaticAberration && !hasChromaticAberrationMacro) {
				shader.AddMacro("CHROMATIC_ABERRATION");
			}
			else if (!postProcessing->chromaticAberration && hasChromaticAberrationMacro) {
				shader.RemoveMacro("CHROMATIC_ABERRATION");
			}

			shader.Bind();

			ivec2 resolution = ivec2(target->GetWidth(), target->GetHeight());

			hdrTextureResolution->SetValue(vec2(resolution));

			exposure->SetValue(camera->exposure);
			saturation->SetValue(postProcessing->saturation);
			timeInMilliseconds->SetValue(1000.0f * Clock::Get());

			if (postProcessing->chromaticAberration) {
				float reversedValue = postProcessing->chromaticAberration->colorsReversed ? 1.0f : 0.0f;
				aberrationStrength->SetValue(postProcessing->chromaticAberration->strength);
				aberrationReversed->SetValue(reversedValue);
			}

			if (postProcessing->vignette) {
				vignetteOffset->SetValue(postProcessing->vignette->offset);
				vignettePower->SetValue(postProcessing->vignette->power);
				vignetteStrength->SetValue(postProcessing->vignette->strength);
				vignetteColor->SetValue(postProcessing->vignette->color);
			}

			if (postProcessing->taa) {
				target->GetHistory()->Bind(GL_TEXTURE0);
			}
			else {
				target->lightingFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT0)->Bind(GL_TEXTURE0);
			}

			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

			target->postProcessFramebuffer.Unbind();

			if (postProcessing->sharpen) {
				sharpenShader.Bind();

				ivec2 groupCount = resolution / 8;

				groupCount.x += ((groupCount.x * 8 == resolution.x) ? 0 : 1);
				groupCount.y += ((groupCount.y * 8 == resolution.y) ? 0 : 1);

				target->postProcessTexture.Bind(GL_WRITE_ONLY, 0);
				target->postProcessFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT0)->Bind(GL_TEXTURE1);

				sharpenFactor->SetValue(postProcessing->sharpen->factor);

				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

				glDispatchCompute(groupCount.x, groupCount.y, 1);

			}

		}

		void PostProcessRenderer::GetUniforms() {

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

		}

	}

}