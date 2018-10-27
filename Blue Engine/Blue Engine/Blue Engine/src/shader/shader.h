#ifndef SHADER_H
#define SHADER_H

#include "../system.h"
#include "shadersource.h"
#include "uniform.h"

#include <list>

class Shader {

public:
	Shader();

	ShaderSource* AddComponent(int32_t type, const char* filename);

	Uniform* GetUniform(const char* uniformName);

	void AddMacro(const char* macro);

	void RemoveMacro(const char* macro);

	bool Compile();

	void Use();

	~Shader();
	
	list<ShaderSource*> shaderComponents;

	bool isCompiled;

private:
	uint32_t ID;

	static uint32_t boundShaderID;

};

#endif