#ifndef SHADER_H
#define SHADER_H

#include "../System.h"
#include "ShaderSource.h"
#include "Uniform.h"

#include <vector>

/*
Proposal for better shader class:
Store all the uniforms, allow uniforms to be returned even if they dont exist
in the current iteration of the shader. If a shader is recompiled or compiled
just update all the uniforms which were previously stored in a vector. Allow
a mode to instantly see shader changes. Therefore check the source files every
time the shader is bound and reload if needed. Activated with ENGINE_INSTANT_SHADER_RELOAD
*/
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
	vector<string> macros;

	bool isCompiled;

private:
	uint32_t ID;	

	static uint32_t boundShaderID;

};

#endif