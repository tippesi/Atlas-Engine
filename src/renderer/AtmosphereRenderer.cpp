#include "AtmosphereRenderer.h"
#include "helper/GeometryHelper.h"

string AtmosphereRenderer::vertexPath = "atmosphere.vsh";
string AtmosphereRenderer::fragmentPath = "atmosphere.fsh";

AtmosphereRenderer::AtmosphereRenderer() {

	vertexArray = GeometryHelper::GenerateSphereVertexArray(20, 20);

	shader = new Shader();

	shader->AddComponent(VERTEX_SHADER, vertexPath);
	shader->AddComponent(FRAGMENT_SHADER, fragmentPath);

	shader->Compile();

	GetUniforms();

}

void AtmosphereRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer) {

	shader->Bind();

	vertexArray->Bind();

	viewProjectionMatrix->SetValue(camera->projectionMatrix * glm::mat4(glm::mat3(camera->viewMatrix)));

	glDrawElements(GL_TRIANGLES, vertexArray->GetIndexComponent()->GetElementCount(),
		vertexArray->GetIndexComponent()->GetDataType(), NULL);

}

void AtmosphereRenderer::GetUniforms() {

	viewProjectionMatrix = shader->GetUniform("vpMatrix");

}