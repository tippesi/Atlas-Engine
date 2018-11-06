#include "directionallightrenderer.h"
#include "masterrenderer.h"

DirectionalLightRenderer::DirectionalLightRenderer(const char* vertexSource, const char* fragmentSource) {

	rectangleVAO = MasterRenderer::GenerateRectangleVAO();

	shader = new Shader();

	shader->AddComponent(VERTEX_SHADER, vertexSource);
	shader->AddComponent(FRAGMENT_SHADER, fragmentSource);

	shader->Compile();

	GetUniforms();

}

void DirectionalLightRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer) {

	shader->Bind();

	vec3 direction = normalize(vec3(camera->viewMatrix * vec4(0.0f, -1.0f, 0.0f, 0.0f)));

	lightDirection->SetValue(direction);
	lightColor->SetValue(vec3(1.0f, 1.0f, 1.0f));
	lightAmbient->SetValue(0.1f);
	inverseViewMatrix->SetValue(camera->inverseViewMatrix);
	inverseProjectionMatrix->SetValue(camera->inverseProjectionMatrix);

	target->geometryFramebuffer->components[0]->Bind(GL_TEXTURE0);
	target->geometryFramebuffer->components[1]->Bind(GL_TEXTURE1);
	target->geometryFramebuffer->components[2]->Bind(GL_TEXTURE2);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

}

void DirectionalLightRenderer::GetUniforms() {

	delete diffuseTexture;
	delete normalTexture;
	delete materialTexture;
	delete depthTexture;
	delete aoTexture;
	delete lightDirection;
	delete lightColor;
	delete lightAmbient;
	delete lightSpaceMatrix;
	delete inverseViewMatrix;
	delete inverseProjectionMatrix;

	diffuseTexture = shader->GetUniform("diffuseTexture");
	normalTexture = shader->GetUniform("normalTexture");
	materialTexture = shader->GetUniform("materialTexture");
	depthTexture = shader->GetUniform("depthTexture");
	aoTexture = shader->GetUniform("aoTexture");
	lightDirection = shader->GetUniform("light.direction");
	lightColor = shader->GetUniform("light.color");
	lightAmbient = shader->GetUniform("light.ambient");
	lightSpaceMatrix = shader->GetUniform("lightSpaceMatrix");
	inverseViewMatrix = shader->GetUniform("ivMatrix");
	inverseProjectionMatrix = shader->GetUniform("ipMatrix");

	diffuseTexture->SetValue(0);
	normalTexture->SetValue(1);
	materialTexture->SetValue(2);
	depthTexture->SetValue(3);
	aoTexture->SetValue(4);

}