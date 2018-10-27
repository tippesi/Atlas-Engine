#ifndef SHADERSOURCE_H
#define SHADERSOURCE_H

#include "../system.h"

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

	bool Compile();

	int32_t ID;
	int32_t type;

	const char* filename;

private:
	string ReadShaderFile(const char* filename);

	string code;
	list<string> macros;

};


#endif