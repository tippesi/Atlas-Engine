#include "Material.h"
#include "renderer/OpaqueRenderer.h"
#include "renderer/ShadowRenderer.h"

namespace Atlas {

	Material::Material() {



	}

	Material::~Material() {

		DeleteTextures();

	}

	Material& Material::operator=(const Material& that) {

		if (this != &that) {

			DeleteTextures();

			diffuseMap = nullptr;
			normalMap = nullptr;
			specularMap = nullptr;
			displacementMap = nullptr;

			// We copy the texture instead of just the pointers
			if (that.diffuseMap)
				diffuseMap = new Texture::Texture2D(*that.diffuseMap);
			if (that.normalMap)
				normalMap = new Texture::Texture2D(*that.normalMap);
			if (that.specularMap)
				specularMap = new Texture::Texture2D(*that.specularMap);
			if (that.displacementMap)
				displacementMap = new Texture::Texture2D(*that.displacementMap);

			diffuseColor = that.diffuseColor;
			specularColor = that.specularColor;
			ambientColor = that.ambientColor;

			specularHardness = that.specularHardness;
			specularIntensity = that.specularIntensity;

			displacementScale = that.displacementScale;

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

	void Material::DeleteTextures() {

		delete diffuseMap;
		delete normalMap;
		delete specularMap;
		delete displacementMap;

	}

}