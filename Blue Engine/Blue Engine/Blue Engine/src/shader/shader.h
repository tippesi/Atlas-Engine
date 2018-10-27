#ifndef SHADER_H
#define SHADER_H

#include "../system.h"
#include "shadersource.h"
#include "uniform.h"

#include <list>

class Shader {

public:
	Shader();

	Uniform* GetUniform(const char* uniformName);

	ShaderSource* AddComponent(int32_t type, const char* filename);

	void AddMacro(const char* macro);

	void RemoveMacro(const char* macro);

	void Compile();

	list<ShaderSource*> shaderComponents;

};

#endif