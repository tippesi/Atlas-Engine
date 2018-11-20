#ifndef SHADER_H
#define SHADER_H

#include "../System.h"
#include "ShaderSource.h"
#include "Uniform.h"

#include <vector>

class Shader {

public:
	Shader();

	void AddComponent(int32_t type, const char* filename);

	void AddComponent(ShaderSource* source);

	ShaderSource* GetComponent(int32_t type);

	Uniform* GetUniform(const char* uniformName);

	void AddMacro(const char* macro);

	void RemoveMacro(const char* macro);

	bool HasMacro(const char* macro);

	bool Compile();

	void Bind();

	~Shader();

	vector<Uniform*> uniforms;
	vector<string> macros;

	bool isCompiled;

private:
	uint32_t ID;

	vector<ShaderSource*> components;

	static uint32_t boundShaderID;

};

#endif