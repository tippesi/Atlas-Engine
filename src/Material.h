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

		bool HasDiffuseMap();
		bool HasNormalMap();
		bool HasSpecularMap();
		bool HasDisplacementMap();

		std::string name;

		Texture::Texture2D* diffuseMap;
		Texture::Texture2D* normalMap;
		Texture::Texture2D* specularMap;
		Texture::Texture2D* displacementMap;

		vec3 diffuseColor;
		vec3 specularColor;
		vec3 ambientColor;

		float specularHardness;
		float specularIntensity;

		float displacementScale;

		std::string diffuseMapPath;
		std::string normalMapPath;
		std::string specularMapPath;
		std::string displacementMapPath;

	};


}

#endif