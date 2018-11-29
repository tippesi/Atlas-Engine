#ifndef MATERIAL_H
#define MATERIAL_H

#include "../System.h"
#include "../Texture.h"
#include "../shader/Shader.h"
#include "../shader/ShaderConfig.h"

class Material {

public:
	Material();

	void Update();

	bool HasDiffuseMap();
	bool HasNormalMap();
	bool HasSpecularMap();
	bool HasDisplacementMap();

	~Material();

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

};

#endif