#include "Material.h"
#include "renderer/GeometryRenderer.h"
#include "renderer/ShadowRenderer.h"

Material::Material() {

	arrayMap = nullptr;
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

	hasDiffuseMap = false;
	hasNormalMap = false;
	hasSpecularMap = false;
	hasDisplacementMap = false;

}

void Material::Update(bool arrayTexture) {

	int32_t width = 0, height = 0, format = 0, layers = 0;

	GeometryRenderer::RemoveConfig(geometryConfig);
	ShadowRenderer::RemoveConfig(shadowConfig);

	geometryConfig->ClearMacros();
	shadowConfig->ClearMacros();

	// Check if we can build an arrayTexture
	if (arrayTexture && !HasArrayMap()) {

		if (HasDiffuseMap()) {
			width = diffuseMap->width;
			height = diffuseMap->height;
			format = diffuseMap->GetSizedDataFormat();
			layers++;
		}

		if (HasNormalMap()) {
			if (format == 0) {
				width = normalMap->width;
				height = normalMap->height;
				format = normalMap->GetSizedDataFormat();
			}
			
			if (normalMap->width != width ||
				normalMap->height != height ||
				normalMap->GetSizedDataFormat() != format) {
				arrayTexture = false;
			}
			layers++;
		}

		if (HasSpecularMap()) {
			if (format == 0) {
				width = specularMap->width;
				height = specularMap->height;
				format = specularMap->GetSizedDataFormat();
			}

			if (specularMap->width != width ||
				specularMap->height != height ||
				specularMap->GetSizedDataFormat() != format) {
				arrayTexture = false;
			}
			layers++;
		}

		if (HasDisplacementMap()) {
			if (format == 0) {
				width = displacementMap->width;
				height = displacementMap->height;
				format = displacementMap->GetSizedDataFormat();
			}

			if (displacementMap->width != width ||
				displacementMap->height != height ||
				displacementMap->GetSizedDataFormat() != format) {
				arrayTexture = false;
			}
			layers++;
		}

	}

	if (arrayTexture && !HasArrayMap()) {

		int32_t layer = 0;
		arrayMap = new Texture(GL_UNSIGNED_BYTE, width, height, format, -0.4f, GL_CLAMP_TO_EDGE, GL_LINEAR, true, true, layers);

		if (HasDiffuseMap()) {
			diffuseMapIndex = (float)layer;
			arrayMap->SetData(diffuseMap->GetData(), layer++);
			delete diffuseMap;
		}

		if (HasNormalMap()) {
			normalMapIndex = (float)layer;
			arrayMap->SetData(normalMap->GetData(), layer++);
			delete normalMap;
		}

		if (HasSpecularMap()) {
			specularMapIndex = (float)layer;
			arrayMap->SetData(specularMap->GetData(), layer++);
			delete specularMap;
		}

		if (HasDisplacementMap()) {
			displacementMapIndex = (float)layer;
			arrayMap->SetData(displacementMap->GetData(), layer++);
			delete displacementMap;
		}

	}	

	if (HasArrayMap()) {
		geometryConfig->AddMacro("ARRAY_MAP");
		shadowConfig->AddMacro("ARRAY_MAP");
	}

	if (HasDiffuseMap()) {
		geometryConfig->AddMacro("DIFFUSE_MAP");
		if (!HasArrayMap()) {
			if (diffuseMap->channels == 4) {
				shadowConfig->AddMacro("ALPHA");
			}
		}
		else {
			if (arrayMap->channels == 4) {
				shadowConfig->AddMacro("ALPHA");
			}
		}
	}
	
	if (HasNormalMap()) {
		geometryConfig->AddMacro("NORMAL_MAP");
	}

	GeometryRenderer::AddConfig(geometryConfig);
	ShadowRenderer::AddConfig(shadowConfig);

}

void Material::AddDiffuseMap(Texture* texture) {

	hasDiffuseMap = true;
	diffuseMap = texture;

}

void Material::AddNormalMap(Texture* texture) {

	hasNormalMap = true;
	normalMap = texture;

}

void Material::AddSpecularMap(Texture* texture) {

	hasSpecularMap = true;
	specularMap = texture;

}

void Material::AddDisplacementMap(Texture* texture) {

	hasDisplacementMap = true;
	displacementMap = texture;

}

bool Material::HasArrayMap() {

	if (arrayMap == nullptr)
		return false;

	return true;

}

bool Material::HasDiffuseMap() {

	return hasDiffuseMap;

}

bool Material::HasNormalMap() {

	return hasNormalMap;

}

bool Material::HasSpecularMap() {

	return hasSpecularMap;

}

bool Material::HasDisplacementMap() {

	return hasDisplacementMap;

}

float Material::GetDiffuseMapIndex() {

	return diffuseMapIndex;

}

float Material::GetNormalMapIndex() {

	return normalMapIndex;

}

float Material::GetSpecularMapIndex() {

	return specularMapIndex;

}

float Material::GetDisplacementMapIndex() {

	return displacementMapIndex;

}

Material::~Material() {

	delete diffuseMap;
	delete normalMap;
	delete specularMap;
	delete displacementMap;

}