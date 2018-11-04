#include "postprocessrenderer.h"
#include "masterrenderer.h"

PostProcessRenderer::PostProcessRenderer(const char* vertexSource, const char* fragmentSoure) {

	rectangleVAO = MasterRenderer::GenerateRectangleVAO();

	shader = new Shader();

	shader->AddComponent(VERTEX_SHADER, vertexSource);
	shader->AddComponent(FRAGMENT_SHADER, fragmentSoure);

	shader->AddMacro("FILMIC_TONEMAPPING");
	shader->AddMacro("CHROMATIC_ABERRATION");

	shader->Compile();

	GetUniforms();

}

void PostProcessRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer) {

	glViewport(window->viewport->x, window->viewport->y, window->viewport->width, window->viewport->height);

	shader->Bind();

	exposure->SetValue(1.0f);
	saturation->SetValue(1.0f);

	aberrationStrength->SetValue(0.7f);
	aberrationReversed->SetValue(1.0f);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	target->postProcessingFramebuffer->components[0]->Bind(GL_TEXTURE0);

	glBindVertexArray(rectangleVAO);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glBindVertexArray(0);

}

void PostProcessRenderer::GetUniforms() {

	delete exposure;
	delete saturation;
	delete bloomPassses;
	delete aberrationStrength;
	delete aberrationReversed;
	delete vignetteOffset;
	delete vignettePower;
	delete vignetteStrength;
	delete vignetteColor;

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