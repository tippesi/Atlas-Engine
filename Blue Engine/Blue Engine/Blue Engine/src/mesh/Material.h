#ifndef MATERIAL_H
#define MATERIAL_H

#include "../System.h"
#include "../Texture.h"
#include "../shader/Shader.h"

class Material {

public:
	Material();

	Shader* GetShader();

	void UpdateShader();

	void Bind(mat4 viewMatrix, mat4 projectionMatrix);
	
	Uniform* GetModelMatrixUniform();

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
	Uniform* normalMapUniform;
	Uniform* specularMapUniform;
	Uniform* heightMapUniform;
	Uniform* modelMatrixUniform;
	Uniform* viewMatrixUniform;
	Uniform* projectionMatrixUniform;

	ShaderConstant* diffuseColorConstant;
	ShaderConstant* specularColorConstant;
	ShaderConstant* ambientColorConstant;
	ShaderConstant* specularHardnessConstant;
	ShaderConstant* specularIntensityConstant;

};

#endif