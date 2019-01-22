#ifndef MATERIAL_H
#define MATERIAL_H

#include "../System.h"
#include "../texture/Texture2D.h"
#include "../texture/Texture2DArray.h"
#include "../loader/ImageLoader.h"
#include "../shader/Shader.h"
#include "../shader/ShaderConfig.h"

#include <unordered_map>

#define MATERIAL_DIFFUSE_MAP 0
#define MATERIAL_NORMAL_MAP 1
#define MATERIAL_SPECULAR_MAP 2
#define MATERIAL_DISPLACEMENT_MAP 3

class Material {

public:
	Material();

	~Material();

	void Update();

	void AddDiffuseMap(Image image);

	void AddNormalMap(Image image);

	void AddSpecularMap(Image image);

	void AddDisplacementMap(Image image);

	bool HasArrayMap();
	bool HasDiffuseMap();
	bool HasNormalMap();
	bool HasSpecularMap();
	bool HasDisplacementMap();

	float GetDiffuseMapIndex();
	float GetNormalMapIndex();
	float GetSpecularMapIndex();
	float GetDisplacementMapIndex();

	Texture2DArray* arrayMap;
	Texture2D* diffuseMap;
	Texture2D* normalMap;
	Texture2D* specularMap;
	Texture2D* displacementMap;

	vec3 diffuseColor;
	vec3 specularColor;
	vec3 ambientColor;

	float specularHardness;
	float specularIntensity;

	float displacementScale;

	ShaderConfig geometryConfig;
	ShaderConfig shadowConfig;

private:
	unordered_map<int32_t, Image> images;

	bool hasDiffuseMap;
	bool hasNormalMap;
	bool hasSpecularMap;
	bool hasDisplacementMap;

	float diffuseMapIndex;
	float normalMapIndex;
	float specularMapIndex;
	float displacementMapIndex;

};

#endif