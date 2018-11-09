#include "Material.h"
#include "renderer/GeometryRenderer.h"

Material::Material() {

	diffuseMap = nullptr;
	normalMap = nullptr;
	specularMap = nullptr;
	heightMap = nullptr;

	diffuseColor = vec3(1.0f);
	specularColor = vec3(1.0f);
	ambientColor = vec3(0.2f);

	specularHardness = 1.0f;
	specularIntensity = 0.0f;

	geometryConfig = new ShaderConfig();

}

void Material::Update() {

	if (geometryConfig != nullptr) {
		// GeometryRenderer::shaderBatch->RemoveConfig(geometryConfig);
	}

	bool hasDiffuseMapMacro = geometryConfig->HasMacro("DIFFUSE_MAP");
	bool hasNormalMapMacro = geometryConfig->HasMacro("NORMAL_MAP");

	if (HasDiffuseMap() && !hasDiffuseMapMacro) {
		geometryConfig->AddMacro("DIFFUSE_MAP");
	}
	else if (!HasDiffuseMap() && hasDiffuseMapMacro){
		geometryConfig->RemoveMacro("DIFFUSE_MAP");
	}
	
	if (HasNormalMap() && !hasNormalMapMacro) {
		geometryConfig->AddMacro("NORMAL_MAP");
	}
	else if (!HasNormalMap() && hasNormalMapMacro){
		geometryConfig->RemoveMacro("NORMAL_MAP");
	}

	GeometryRenderer::shaderBatch->AddConfig(geometryConfig);

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

}