#include "AtmosphereRenderer.h"
#include "helper/GeometryHelper.h"

AtmosphereRenderer::AtmosphereRenderer(string vertexSource, string fragmentSource) {

	vertexArray = GeometryHelper::GenerateSphereVertexArray(20, 20);

	shader = new Shader();

	shader->AddComponent(VERTEX_SHADER, vertexSource);
	shader->AddComponent(FRAGMENT_SHADER, fragmentSource);

	shader->Compile();

	GetUniforms();

}

void AtmosphereRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer) {

	glDisable(GL_CULL_FACE);

	shader->Bind();

	vertexArray->Bind();

	modelViewProjectionMatrix->SetValue(camera->projectionMatrix * glm::mat4(glm::mat3(camera->viewMatrix)));

	glDrawElements(GL_TRIANGLES, vertexArray->GetIndexComponent()->GetElementCount(),
		vertexArray->GetIndexComponent()->GetDataType(), NULL);

	glEnable(GL_CULL_FACE);

}

void AtmosphereRenderer::GetUniforms() {

	modelViewProjectionMatrix = shader->GetUniform("mvpMatrix");

}