#ifndef AE_MATERIAL_H
#define AE_MATERIAL_H

#include "../System.h"
#include "../texture/Texture2D.h"
#include "../texture/Texture2DArray.h"
#include "../loader/ImageLoader.h"
#include "../shader/Shader.h"
#include "../shader/ShaderConfig.h"

#include <unordered_map>

#define AE_MATERIAL_DIFFUSE_MAP 0
#define AE_MATERIAL_NORMAL_MAP 1
#define AE_MATERIAL_SPECULAR_MAP 2
#define AE_MATERIAL_DISPLACEMENT_MAP 3

namespace Atlas {

	namespace Mesh {

		class Material {

		public:
			Material();

			~Material();

			void Update();

			void AddDiffuseMap(Loader::Image image);

			void AddNormalMap(Loader::Image image);

			void AddSpecularMap(Loader::Image image);

			void AddDisplacementMap(Loader::Image image);

			bool HasArrayMap();
			bool HasDiffuseMap();
			bool HasNormalMap();
			bool HasSpecularMap();
			bool HasDisplacementMap();

			float GetDiffuseMapIndex();
			float GetNormalMapIndex();
			float GetSpecularMapIndex();
			float GetDisplacementMapIndex();

			Texture::Texture2DArray* arrayMap;
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

			Shader::ShaderConfig geometryConfig;
			Shader::ShaderConfig shadowConfig;

		private:
			std::unordered_map<int32_t, Loader::Image> images;

			bool hasDiffuseMap;
			bool hasNormalMap;
			bool hasSpecularMap;
			bool hasDisplacementMap;

			float diffuseMapIndex;
			float normalMapIndex;
			float specularMapIndex;
			float displacementMapIndex;

		};

	}

}

#endif