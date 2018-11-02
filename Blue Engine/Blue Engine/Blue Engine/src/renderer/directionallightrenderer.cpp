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

}