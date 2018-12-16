#ifndef MATERIAL_H
#define MATERIAL_H

#include "../System.h"
#include "../Texture.h"
#include "../shader/Shader.h"
#include "../shader/ShaderConfig.h"

class Material {

public:
	Material();

	void Update(bool arrayTexture = true);

	void AddDiffuseMap(Texture* texture);

	void AddNormalMap(Texture* texture);

	void AddSpecularMap(Texture* texture);

	void AddDisplacementMap(Texture* texture);

	bool HasArrayMap();
	bool HasDiffuseMap();
	bool HasNormalMap();
	bool HasSpecularMap();
	bool HasDisplacementMap();

	float GetDiffuseMapIndex();
	float GetNormalMapIndex();
	float GetSpecularMapIndex();
	float GetDisplacementMapIndex();

	~Material();

	Texture* arrayMap;
	Texture* diffuseMap;
	Texture* normalMap;
	Texture* specularMap;
	Texture* displacementMap;

	vec3 diffuseColor;
	vec3 specularColor;
	vec3 ambientColor;

	float specularHardness;
	float specularIntensity;

	float displacementScale;

	ShaderConfig* geometryConfig;
	ShaderConfig* shadowConfig;

private:
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