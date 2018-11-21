#ifndef SHADERSOURCE_H
#define SHADERSOURCE_H

#include "../System.h"
#include "ShaderConstant.h"

#include <list>
#include <string>

#define VERTEX_SHADER GL_VERTEX_SHADER
#define FRAGMENT_SHADER GL_FRAGMENT_SHADER
#define GEOMETRY_SHADER GL_GEOMETRY_SHADER
#define TESSELATION_CONTROL_SHADER GL_TESS_CONTROL_SHADER
#define TESSELATION_EVALUATION_SHADER GL_TESS_EVALUATION_SHADER

class ShaderSource {

public:
	ShaderSource(int32_t type, string filename);

	ShaderSource(ShaderSource* source);

	bool Reload();

	void AddMacro(string macro);

	void RemoveMacro(string macro);

	ShaderConstant* GetConstant(string constant);

	bool Compile();

	static void SetSourceDirectory(string directory);

	~ShaderSource();

	int32_t ID;
	int32_t type;

	string filename;

private:
	string ReadShaderFile(string filename, bool mainFile);

	time_t GetLastModified();

	string code;
	list<string> macros;
	list<ShaderConstant*> constants;

	time_t lastModified;

	static string sourceDirectory;

};


#endif