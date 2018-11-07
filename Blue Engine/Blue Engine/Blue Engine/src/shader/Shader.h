#ifndef SHADER_H
#define SHADER_H

#include "../System.h"
#include "ShaderSource.h"
#include "Uniform.h"

#include <vector>

class Shader {

public:
	Shader();

	ShaderSource* AddComponent(int32_t type, const char* filename);

	void AddComponent(ShaderSource* source);

	Uniform* GetUniform(const char* uniformName);

	void AddMacro(const char* macro);

	void RemoveMacro(const char* macro);

	bool HasMacro(const char* macro);

	bool Compile();

	void Bind();

	~Shader();
	
	vector<ShaderSource*> components;

	bool isCompiled;

private:
	uint32_t ID;

	vector<string> macros;

	static uint32_t boundShaderID;

};

#endif