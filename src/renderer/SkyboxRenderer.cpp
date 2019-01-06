#include "SkyboxRenderer.h"
#include "helper/GeometryHelper.h"

string SkyboxRenderer::vertexPath = "skybox.vsh";
string SkyboxRenderer::fragmentPath = "skybox.fsh";

SkyboxRenderer::SkyboxRenderer() {

	shader = new Shader();

	shader->AddStage(VERTEX_SHADER, vertexPath);
	shader->AddStage(FRAGMENT_SHADER, fragmentPath);

	shader->Compile();

	GetUniforms();

	vertexArray = GeometryHelper::GenerateCubeVertexArray();

}

void SkyboxRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene) {

	shader->Bind();

	skyCubemap->SetValue(0);

	mat4 mvpMatrix = camera->projectionMatrix * glm::mat4(glm::mat3(camera->viewMatrix)) * scene->sky->skybox->matrix;

	modelViewProjectionMatrix->SetValue(mvpMatrix);

	vertexArray->Bind();

	scene->sky->skybox->cubemap->Bind(GL_TEXTURE0);

	glDrawArrays(GL_TRIANGLES, 0, 36);	

}

void SkyboxRenderer::GetUniforms() {

	skyCubemap = shader->GetUniform("skyCubemap");
	modelViewProjectionMatrix = shader->GetUniform("mvpMatrix");

}