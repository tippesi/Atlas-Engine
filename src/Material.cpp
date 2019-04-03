#include "Material.h"
#include "renderer/OpaqueRenderer.h"
#include "renderer/ShadowRenderer.h"

namespace Atlas {

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

	}

	Material::~Material() {

		delete diffuseMap;
		delete normalMap;
		delete specularMap;
		delete displacementMap;

	}

	bool Material::HasDiffuseMap() {

		return diffuseMap ? true : false;

	}

	bool Material::HasNormalMap() {

		return normalMap ? true : false;

	}

	bool Material::HasSpecularMap() {

		return specularMap ? true : false;

	}

	bool Material::HasDisplacementMap() {

		return displacementMap ? true : false;

	}

}