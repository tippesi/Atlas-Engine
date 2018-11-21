#ifndef SHADER_H
#define SHADER_H

#include "../System.h"
#include "ShaderSource.h"
#include "Uniform.h"

#include <vector>

class Shader {

public:
	Shader();

	void AddComponent(int32_t type, string filename);

	void AddComponent(ShaderSource* source);

	ShaderSource* GetComponent(int32_t type);

	Uniform* GetUniform(string uniformName);

	void AddMacro(string macro);

	void RemoveMacro(string macro);

	bool HasMacro(string macro);

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