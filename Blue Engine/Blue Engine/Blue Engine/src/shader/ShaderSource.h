#ifndef SHADERSOURCE_H
#define SHADERSOURCE_H

#include "../System.h"
#include "ShaderConstant.h"

#include <list>
#include <string>

#define VERTEX_SHADER GL_VERTEX_SHADER
#define FRAGMENT_SHADER GL_FRAGMENT_SHADER
#define GEOMETRY_SHADER GL_GEOMETRY_SHADER

class ShaderSource {

public:
	ShaderSource(int32_t type, const char* filename);

	void AddMacro(const char* macro);

	void RemoveMacro(const char* macro);

	ShaderConstant* GetConstant(const char* constant);

	bool Compile();

	~ShaderSource();

	int32_t ID;
	int32_t type;

	const char* filename;

private:
	string ReadShaderFile(const char* filename, bool mainFile);

	string code;
	list<string> macros;
	list<ShaderConstant*> constants;

};


#endif