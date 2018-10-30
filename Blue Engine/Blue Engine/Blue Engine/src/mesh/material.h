#ifndef MATERIAL_H
#define MATERIAL_H

#include "../system.h"
#include "../texture.h"
#include "../shader/shader.h"

class Material {

public:
	Material();

	Shader* GetShader();

	void UpdateShader();

	bool HasDiffuseMap();
	bool HasNormalMap();
	bool HasSpecularMap();
	bool HasHeightMap();

	~Material();

	Texture* diffuseMap;
	Texture* normalMap;
	Texture* specularMap;
	Texture* heightMap;

	vec3 diffuseColor;
	vec3 specularColor;
	vec3 ambientColor;

	float specularHardness;
	float specularIntensity;

private:
	Shader* shader;

	Uniform* diffuseMapUniform;

};

#endif