#include "Material.h"

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

	bool Material::HasBaseColorMap() const {

		return baseColorMap ? true : false;

	}

	bool Material::HasOpacityMap() const {

		return opacityMap ? true : false;

	}

	bool Material::HasNormalMap() const {

		return normalMap ? true : false;

	}

	bool Material::HasRoughnessMap() const {

		return roughnessMap ? true : false;

	}

	bool Material::HasMetalnessMap() const {

		return metalnessMap ? true : false;

	}

	bool Material::HasAoMap() const {

		return aoMap ? true : false;

	}

	bool Material::HasDisplacementMap() const {

		return displacementMap ? true : false;

	}

	void Material::DeepCopy(const Material& that) {

		DeleteTextures();

		name = that.name;

		baseColorMap = nullptr;
		normalMap = nullptr;
		roughnessMap = nullptr;
		metalnessMap = nullptr;
		aoMap = nullptr;
		displacementMap = nullptr;

		// We copy the texture instead of just the pointers
		if (that.baseColorMap) {
			baseColorMap = new Texture::Texture2D(*that.baseColorMap);
			baseColorMapPath = that.baseColorMapPath;
		}
		if (that.opacityMap) {
			opacityMap = new Texture::Texture2D(*that.opacityMap);
			opacityMapPath = that.opacityMapPath;
		}
		if (that.normalMap) {
			normalMap = new Texture::Texture2D(*that.normalMap);
			normalMapPath = that.normalMapPath;
		}
		if (that.roughnessMap) {
			roughnessMap = new Texture::Texture2D(*that.roughnessMap);
			roughnessMapPath = that.roughnessMapPath;
		}
		if (that.metalnessMap) {
			metalnessMap = new Texture::Texture2D(*that.metalnessMap);
			metalnessMapPath = that.metalnessMapPath;
		}
		if (that.aoMap) {
			aoMap = new Texture::Texture2D(*that.aoMap);
			aoMapPath = that.aoMapPath;
		}
		if (that.displacementMap) {
			displacementMap = new Texture::Texture2D(*that.displacementMap);
			displacementMapPath = that.displacementMapPath;
		}

		baseColor = that.baseColor;
		emissiveColor = that.emissiveColor;
		transmissiveColor = that.transmissiveColor;

		opacity = that.opacity;

		roughness = that.roughness;
		metalness = that.metalness;
		ao = that.ao;

		reflectance = that.reflectance;

		normalScale = that.normalScale;
		displacementScale = that.displacementScale;

		twoSided = that.twoSided;

	}

	void Material::DeleteTextures() {

		delete baseColorMap;
		delete opacityMap;
		delete normalMap;
		delete roughnessMap;
		delete metalnessMap;
		delete aoMap;
		delete displacementMap;

	}

}