#include "postprocessrenderer.h"
#include "masterrenderer.h"

PostProcessRenderer::PostProcessRenderer(const char* vertexSource, const char* fragmentSoure) {

	rectangleVAO = MasterRenderer::GenerateRectangleVAO();

	shader = new Shader();

	shader->AddComponent(VERTEX_SHADER, vertexSource);
	shader->AddComponent(FRAGMENT_SHADER, fragmentSoure);

	shader->AddMacro("FILMIC_TONEMAPPING");

	shader->Compile();

	GetUniforms();

}

void PostProcessRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer) {

	glViewport(window->viewport->x, window->viewport->y, window->viewport->width, window->viewport->height);

	shader->Bind();

	exposure->SetValue(1.0f);
	saturation->SetValue(1.0f);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	target->geometryFramebuffer->components[1]->Bind(GL_TEXTURE0);

	glBindVertexArray(rectangleVAO);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glBindVertexArray(0);

}

void PostProcessRenderer::GetUniforms() {

	delete exposure;
	delete saturation;

	exposure = shader->GetUniform("exposure");
	saturation = shader->GetUniform("saturation");

}