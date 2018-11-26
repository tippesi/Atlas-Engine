#ifndef SHADER_H
#define SHADER_H

#include "../System.h"
#include "ShaderSource.h"
#include "Uniform.h"

#include <vector>

class Shader {

public:
	///
	Shader();

	///
	/// \param type
	/// \param filename
	void AddComponent(int32_t type, string filename);

	///
	/// \param source
	void AddComponent(ShaderSource* source);

	///
	/// \param type
	/// \return
	ShaderSource* GetComponent(int32_t type);

	///
	/// \param uniformName
	/// \return
	Uniform* GetUniform(string uniformName);

	///
	/// \param macro
	void AddMacro(string macro);

	///
	/// \param macro
	void RemoveMacro(string macro);

	///
	/// \param macro
	/// \return
	bool HasMacro(string macro);

	///
	/// \return
	bool Compile();

	///
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