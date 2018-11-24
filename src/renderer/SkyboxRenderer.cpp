#include "SkyboxRenderer.h"
#include "helper/GeometryHelper.h"

SkyboxRenderer::SkyboxRenderer(string vertexSource, string fragmentSource) {

	shader = new Shader();

	shader->AddComponent(VERTEX_SHADER, vertexSource);
	shader->AddComponent(FRAGMENT_SHADER, fragmentSource);

	shader->Compile();

	skyCubemap = shader->GetUniform("skyCubemap");
	modelViewProjectionMatrix = shader->GetUniform("mvpMatrix");

	vertexArray = GeometryHelper::GenerateCubeVertexArray();

}

void SkyboxRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer) {

	shader->Bind();

	skyCubemap->SetValue(0);

	mat4 mvpMatrix = camera->projectionMatrix * glm::mat4(glm::mat3(camera->viewMatrix)) * scene->sky->skybox->matrix;

	modelViewProjectionMatrix->SetValue(mvpMatrix);

	vertexArray->Bind();

	scene->sky->skybox->cubemap->Bind(GL_TEXTURE0);

	glDrawArrays(GL_TRIANGLES, 0, 36);	

}