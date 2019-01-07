#include "SkyboxRenderer.h"
#include "helper/GeometryHelper.h"

string SkyboxRenderer::vertexPath = "skybox.vsh";
string SkyboxRenderer::fragmentPath = "skybox.fsh";

SkyboxRenderer::SkyboxRenderer() {

	GeometryHelper::GenerateCubeVertexArray(vertexArray);

	shader.AddStage(VERTEX_STAGE, vertexPath);
	shader.AddStage(FRAGMENT_STAGE, fragmentPath);

	shader.Compile();

	GetUniforms();

}

void SkyboxRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene) {

	shader.Bind();

	skyCubemap->SetValue(0);

	mat4 mvpMatrix = camera->projectionMatrix * glm::mat4(glm::mat3(camera->viewMatrix)) * scene->sky->skybox->matrix;

	modelViewProjectionMatrix->SetValue(mvpMatrix);

	vertexArray.Bind();

	scene->sky->skybox->cubemap->Bind(GL_TEXTURE0);

	glDrawArrays(GL_TRIANGLES, 0, 36);	

}

void SkyboxRenderer::GetUniforms() {

	skyCubemap = shader.GetUniform("skyCubemap");
	modelViewProjectionMatrix = shader.GetUniform("mvpMatrix");

}