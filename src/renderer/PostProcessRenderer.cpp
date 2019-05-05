#include "PostProcessRenderer.h"
#include "MasterRenderer.h"

#include "../Clock.h"

namespace Atlas {

	namespace Renderer {

		std::string PostProcessRenderer::vertexPath = "postprocessing.vsh";
		std::string PostProcessRenderer::fragmentPath = "postprocessing.fsh";

		PostProcessRenderer::PostProcessRenderer() {

			shader.AddStage(AE_VERTEX_STAGE, vertexPath);
			shader.AddStage(AE_FRAGMENT_STAGE, fragmentPath);

			shader.Compile();

			GetUniforms();

		}

		void PostProcessRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

			glViewport(viewport->x, viewport->y, viewport->width, viewport->height);

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

			if (postProcessing->vignette != nullptr && !hasVignetteMacro) {
				shader.AddMacro("VIGNETTE");
			}
			else if (postProcessing->vignette == nullptr  && hasVignetteMacro) {
				shader.RemoveMacro("VIGNETTE");
			}

			if (postProcessing->chromaticAberration != nullptr && !hasChromaticAberrationMacro) {
				shader.AddMacro("CHROMATIC_ABERRATION");
			}
			else if (postProcessing->chromaticAberration == nullptr && hasChromaticAberrationMacro) {
				shader.RemoveMacro("CHROMATIC_ABERRATION");
			}

			shader.Bind();

			hdrTextureResolution->SetValue(
					vec2(target->lightingFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT0)->width,
						 target->lightingFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT0)->height));

			exposure->SetValue(postProcessing->exposure);
			saturation->SetValue(postProcessing->saturation);
			timeInMilliseconds->SetValue(1000.0f * Clock::Get());

			if (postProcessing->chromaticAberration != nullptr) {
				float reversedValue = postProcessing->chromaticAberration->colorsReversed ? 1.0f : 0.0f;
				aberrationStrength->SetValue(postProcessing->chromaticAberration->strength);
				aberrationReversed->SetValue(reversedValue);
			}

			if (postProcessing->vignette != nullptr) {
				vignetteOffset->SetValue(postProcessing->vignette->offset);
				vignettePower->SetValue(postProcessing->vignette->power);
				vignetteStrength->SetValue(postProcessing->vignette->strength);
				vignetteColor->SetValue(postProcessing->vignette->color);
			}

			target->lightingFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT0)->Bind(GL_TEXTURE0);

			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

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