#include "material.h"

Material::Material() {

	shader = new Shader();
	shader->AddComponent(VERTEX_SHADER, "shader/deferred/geometry.vsh");
	ShaderSource* fragment = shader->AddComponent(FRAGMENT_SHADER, "shader/deferred/geometry.fsh");

	diffuseColorConstant = fragment->GetConstant("diffuseColor");
	specularColorConstant = fragment->GetConstant("specularColor");
	ambientColorConstant = fragment->GetConstant("ambientColor");
	specularHardnessConstant = fragment->GetConstant("specularHardness");
	specularIntensityConstant = fragment->GetConstant("specularIntensity");

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

	delete diffuseMapUniform;
	delete specularMapUniform;
	delete normalMapUniform;
	delete heightMapUniform;
	delete modelMatrixUniform;
	delete viewMatrixUniform;
	delete projectionMatrixUniform;

	diffuseColorConstant->SetValue(diffuseColor);
	specularHardnessConstant->SetValue(specularHardness);
	specularIntensityConstant->SetValue(specularIntensity);

	if (HasDiffuseMap() && !shader->HasMacro("DIFFUSE_MAP")) {
		shader->AddMacro("DIFFUSE_MAP");
	}
	else {
		shader->RemoveMacro("DIFFUSE_MAP");
	}

	if (HasNormalMap() && !shader->HasMacro("NORMAL_MAP")) {
		shader->AddMacro("NORMAL_MAP");
	}
	else {
		shader->RemoveMacro("NORMAL_MAP");
	}

	shader->Compile();

	diffuseMapUniform = shader->GetUniform("diffuseMap");
	specularMapUniform = shader->GetUniform("specularMap");
	normalMapUniform = shader->GetUniform("normalMap");
	heightMapUniform = shader->GetUniform("heightMap");
	modelMatrixUniform = shader->GetUniform("mMatrix");
	viewMatrixUniform = shader->GetUniform("vMatrix");
	projectionMatrixUniform = shader->GetUniform("pMatrix");

	diffuseMapUniform->SetValue(0);
	normalMapUniform->SetValue(1);
	specularMapUniform->SetValue(2);
	heightMapUniform->SetValue(3);

}

void Material::Bind(mat4 viewMatrix, mat4 projectionMatrix) {

	shader->Bind();

	viewMatrixUniform->SetValue(viewMatrix);
	projectionMatrixUniform->SetValue(projectionMatrix);

	if (HasDiffuseMap())
		diffuseMap->Bind(GL_TEXTURE0);
	if (HasNormalMap())
		normalMap->Bind(GL_TEXTURE1);
	if (HasSpecularMap())
		specularMap->Bind(GL_TEXTURE2);
	if (HasHeightMap())
		heightMap->Bind(GL_TEXTURE3);

}

Uniform* Material::GetModelMatrixUniform() {

	return modelMatrixUniform;

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

	delete diffuseMapUniform;
	delete specularMapUniform;
	delete normalMapUniform;
	delete heightMapUniform;
	delete modelMatrixUniform;
	delete viewMatrixUniform;
	delete projectionMatrixUniform;

	delete diffuseColorConstant;
	delete specularColorConstant;
	delete ambientColorConstant;
	delete specularHardnessConstant;
	delete specularIntensityConstant;

}