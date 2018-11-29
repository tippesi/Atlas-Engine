#include "Material.h"
#include "renderer/GeometryRenderer.h"
#include "renderer/DirectionalShadowRenderer.h"

Material::Material() {

	diffuseMap = nullptr;
	normalMap = nullptr;
	specularMap = nullptr;
	displacementMap = nullptr;

	diffuseColor = vec3(1.0f);
	specularColor = vec3(1.0f);
	ambientColor = vec3(0.2f);

	specularHardness = 1.0f;
	specularIntensity = 0.0f;

	displacementScale = 1.0f;

	geometryConfig = new ShaderConfig();
	shadowConfig = new ShaderConfig();

}

void Material::Update() {

	GeometryRenderer::RemoveConfig(geometryConfig);
	DirectionalShadowRenderer::RemoveConfig(shadowConfig);

	geometryConfig->ClearMacros();
	shadowConfig->ClearMacros();

	if (HasDiffuseMap()) {
		geometryConfig->AddMacro("DIFFUSE_MAP");
		if (diffuseMap->channels == 4 && shadowConfig) {
			shadowConfig->AddMacro("ALPHA");
		}
	}
	
	if (HasNormalMap()) {
		geometryConfig->AddMacro("NORMAL_MAP");
	}

	GeometryRenderer::AddConfig(geometryConfig);
	DirectionalShadowRenderer::AddConfig(shadowConfig);

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

bool Material::HasDisplacementMap() {

	if (displacementMap == nullptr)
		return false;

	return true;

}

Material::~Material() {

	delete diffuseMap;
	delete normalMap;
	delete specularMap;
	delete displacementMap;

}