#ifndef AE_MATERIAL_H
#define AE_MATERIAL_H

#include "System.h"
#include "texture/Texture2D.h"
#include "texture/Texture2DArray.h"
#include "loader/ImageLoader.h"
#include "shader/Shader.h"
#include "shader/ShaderConfig.h"

#include <string>

namespace Atlas {

	class Material {

	public:
		Material();

		~Material();

		Material& operator=(const Material& that);

		bool HasDiffuseMap() const;
		bool HasNormalMap() const;
		bool HasSpecularMap() const;
		bool HasDisplacementMap() const;

		std::string name;

		Texture::Texture2D* diffuseMap = nullptr;
		Texture::Texture2D* normalMap = nullptr;
		Texture::Texture2D* specularMap = nullptr;
		Texture::Texture2D* displacementMap = nullptr;

		vec3 diffuseColor = vec3(1.0f);
		vec3 specularColor = vec3(1.0f);
		vec3 ambientColor = vec3(1.0f);
		vec3 emissiveColor = vec3(0.0f);

		float specularHardness = 1.0f;
		float specularIntensity = 0.0f;

		float displacementScale = 1.0f;

		std::string diffuseMapPath;
		std::string normalMapPath;
		std::string specularMapPath;
		std::string displacementMapPath;

	private:
		void DeleteTextures();

	};


}

#endif