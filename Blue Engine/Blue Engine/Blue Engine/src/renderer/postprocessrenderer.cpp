#include "postprocessrenderer.h"
#include "masterrenderer.h"

PostProcessRenderer::PostProcessRenderer(const char* vertexSource, const char* fragmentSoure) {

	rectangleVAO = MasterRenderer::GenerateRectangleVAO();

	shader = new Shader();

	shader->AddComponent(VERTEX_SHADER, vertexSource);
	shader->AddComponent(FRAGMENT_SHADER, fragmentSoure);

	shader->AddMacro("FILMIC_TONEMAPPING");

	shader->Compile();

	exposure = shader->GetUniform("exposure");
	saturation = shader->GetUniform("saturation");

}

void PostProcessRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene) {

	glViewport(window->viewport->x, window->viewport->y, window->viewport->width, window->viewport->height);

	shader->Bind();

	exposure->SetValue(1.0f);
	saturation->SetValue(1.0f);

	target->texture->Bind(GL_TEXTURE0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindVertexArray(rectangleVAO);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glBindVertexArray(0);

}