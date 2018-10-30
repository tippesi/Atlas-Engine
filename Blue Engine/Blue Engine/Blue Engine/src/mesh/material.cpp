#include "material.h"

Material::Material() {

	shader = new Shader();
	shader->AddComponent(VERTEX_SHADER, "blub.vsh");
	shader->AddComponent(FRAGMENT_SHADER, "blub.fsh");

	diffuseMap = nullptr;
	normalMap = nullptr;
	specularMap = nullptr;
	heightMap = nullptr;

	diffuseColor = vec3(1.0f);
	specularColor = vec3(1.0f);
	ambientColor = vec3(0.2f);

	specularHardness = 1.0f;
	specularIntensity = 0.0f;

}

Shader* Material::GetShader() {

	return shader;

}

void Material::UpdateShader() {



}

bool Material::HasDiffuseMap() {

	if (diffuseMap == nullptr)
		return false;

	return true;

}

bool Material::HasNormalMap() {

	if (normalMap == nullptr)
		return false;

	return true;

}

bool Material::HasSpecularMap() {

	if (specularMap == nullptr)
		return false;

	return true;

}

bool Material::HasHeightMap() {

	if (heightMap == nullptr)
		return false;

	return true;

}

Material::~Material() {

	delete diffuseMap;
	delete normalMap;
	delete specularMap;
	delete heightMap;

	delete shader;

}