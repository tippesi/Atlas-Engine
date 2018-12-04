#include "PostProcessRenderer.h"
#include "MasterRenderer.h"

string PostProcessRenderer::vertexPath = "postprocessing.vsh";
string PostProcessRenderer::fragmentPath = "postprocessing.fsh";

PostProcessRenderer::PostProcessRenderer() {

	shader = new Shader();

	shader->AddComponent(VERTEX_SHADER, vertexPath);
	shader->AddComponent(FRAGMENT_SHADER, fragmentPath);

	shader->Compile();

	GetUniforms();

}

void PostProcessRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer) {

	glViewport(window->viewport->x, window->viewport->y, window->viewport->width, window->viewport->height);

	PostProcessing* postProcessing = scene->postProcessing;

	bool shaderChanged = false;

	bool hasFilmicTonemappingMacro = shader->HasMacro("FILMIC_TONEMAPPING");
	bool hasVignetteMacro = shader->HasMacro("VIGNETTE");
	bool hasChromaticAberrationMacro = shader->HasMacro("CHROMATIC_ABERRATION");

	if (postProcessing->filmicTonemapping && !hasFilmicTonemappingMacro) {
		shader->AddMacro("FILMIC_TONEMAPPING");
		shaderChanged = true;
	}
	else if (!postProcessing->filmicTonemapping && hasFilmicTonemappingMacro) {
		shader->RemoveMacro("FILMIC_TONEMAPPING");
		shaderChanged = true;
	}

	if (postProcessing->vignette != nullptr && !hasVignetteMacro) {
		shader->AddMacro("VIGNETTE");
		shaderChanged = true;
	}
	else if (postProcessing->vignette == nullptr  && hasVignetteMacro) {
		shader->RemoveMacro("VIGNETTE");
		shaderChanged = true;
	}

	if (postProcessing->chromaticAberration != nullptr && !hasChromaticAberrationMacro) {
		shader->AddMacro("CHROMATIC_ABERRATION");
		shaderChanged = true;
	}
	else if (postProcessing->chromaticAberration == nullptr && hasChromaticAberrationMacro) {
		shader->RemoveMacro("CHROMATIC_ABERRATION");
		shaderChanged = true;
	}

	if (shaderChanged) {
		shader->Compile();
	}

	shader->Bind();

	hdrTexture->SetValue(0);
	bloomFirstTexture->SetValue(1);
	bloomSecondTexture->SetValue(2);
	bloomThirdTexture->SetValue(3);

	exposure->SetValue(postProcessing->exposure);
	saturation->SetValue(postProcessing->saturation);

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

	target->lightingFramebuffer->GetComponentTexture(GL_COLOR_ATTACHMENT0)->Bind(GL_TEXTURE0);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

}

void PostProcessRenderer::GetUniforms() {

	hdrTexture = shader->GetUniform("hdrTexture");
	bloomFirstTexture = shader->GetUniform("bloomFirstTexture");
	bloomSecondTexture = shader->GetUniform("bloomSecondTexture");
	bloomThirdTexture = shader->GetUniform("bloomThirdTexture");
	exposure = shader->GetUniform("exposure");
	saturation = shader->GetUniform("saturation");
	bloomPassses = shader->GetUniform("bloomPassses");
	aberrationStrength = shader->GetUniform("aberrationStrength");
	aberrationReversed = shader->GetUniform("aberrationReversed");
	vignetteOffset = shader->GetUniform("vignetteOffset");
	vignettePower = shader->GetUniform("vignettePower");
	vignetteStrength = shader->GetUniform("vignetteStrength");
	vignetteColor = shader->GetUniform("vignetteColor");

}