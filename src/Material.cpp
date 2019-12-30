#include "Material.h"
#include "renderer/OpaqueRenderer.h"
#include "renderer/ShadowRenderer.h"

namespace Atlas {

	Material::Material() {



	}

	Material::Material(const Material& that) {

		DeepCopy(that);

	}

	Material::~Material() {

		DeleteTextures();

	}

	Material& Material::operator=(const Material& that) {

		if (this != &that) {

			DeepCopy(that);

		}

		return *this;

	}

	bool Material::HasDiffuseMap() const {

		return diffuseMap ? true : false;

	}

	bool Material::HasNormalMap() const {

		return normalMap ? true : false;

	}

	bool Material::HasSpecularMap() const {

		return specularMap ? true : false;

	}

	bool Material::HasDisplacementMap() const {

		return displacementMap ? true : false;

	}

	void Material::DeepCopy(const Material& that) {

		DeleteTextures();

		diffuseMap = nullptr;
		normalMap = nullptr;
		specularMap = nullptr;
		displacementMap = nullptr;

		// We copy the texture instead of just the pointers
		if (that.diffuseMap) {
			diffuseMap = new Texture::Texture2D(*that.diffuseMap);
			diffuseMapPath = that.diffuseMapPath;
		}
		if (that.normalMap) {
			normalMap = new Texture::Texture2D(*that.normalMap);
			normalMapPath = that.normalMapPath;
		}
		if (that.specularMap) {
			specularMap = new Texture::Texture2D(*that.specularMap);
			specularMapPath = that.specularMapPath;
		}
		if (that.displacementMap) {
			displacementMap = new Texture::Texture2D(*that.displacementMap);
			displacementMapPath = that.displacementMapPath;
		}

		diffuseColor = that.diffuseColor;
		specularColor = that.specularColor;
		ambientColor = that.ambientColor;
		emissiveColor = that.emissiveColor;

		specularHardness = that.specularHardness;
		specularIntensity = that.specularIntensity;

		normalScale = that.normalScale;
		displacementScale = that.displacementScale;

	}

	void Material::DeleteTextures() {

		delete diffuseMap;
		delete normalMap;
		delete specularMap;
		delete displacementMap;

	}

}