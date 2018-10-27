#include "shadersource.h"
#include <fstream>
#include <sstream>

ShaderSource::ShaderSource(int32_t type, const char* filename) : type(type), filename(filename) {

	code = ReadShaderFile(filename);
	ID = glCreateShader(type);

#ifdef ENGINE_SHOW_LOG
	EngineLog("Loaded shader file %s", filename);
#endif
}

void ShaderSource::AddMacro(const char* macro) {

	macros.push_back(string(macro));

}

void ShaderSource::RemoveMacro(const char* macro) {

	macros.remove(string(macro));

}

bool ShaderSource::Compile() {

	string composedCode;

#ifdef ENGINE_OGL
	composedCode.append("#version 400 compatibility\n\n#define ENGINE_OGL\n");
#else
	composedCode.append("#version 300 es\n\nprecision highp float;\n#define ENGINE_GLES\n")
#endif

	for (list<string>::iterator iterator = macros.begin(); iterator != macros.end(); iterator++) {
		composedCode.append("#define " + *iterator + "\n");
	}

	composedCode.append(code);

	const char* convertedCode = composedCode.c_str();

	int compiled = 0;

	glShaderSource(ID, 1, &convertedCode, 0);
	glCompileShader(ID);
	glGetShaderiv(ID, GL_COMPILE_STATUS, &compiled);

	if (!compiled) {
#ifdef ENGINE_SHOW_LOG
		int32_t shaderLogLength, length;
		glGetShaderiv(ID, GL_INFO_LOG_LENGTH, &shaderLogLength);
		char *shaderLog = new char[shaderLogLength];
		glGetShaderInfoLog(ID, shaderLogLength, &length, shaderLog);

		if (type == GL_VERTEX_SHADER) {
			EngineLog("\n\nCompiling Vertexshader failed:");
		}
		else {
			EngineLog("\n\nCompiling Fragmentshader failed:");
		}

		EngineLog("Compilation failed: %s\nError: %s", filename, shaderLog);

		delete shaderLog;
#endif

		return false;

	}

	return true;

}

string ShaderSource::ReadShaderFile(const char* filename) {

	string shaderCode;
	ifstream shaderFile;
	stringstream shaderStream;

	shaderFile.open(filename, ios::in);

	if (shaderFile.is_open()) {	
		shaderStream << shaderFile.rdbuf();
		shaderFile.close();
		shaderCode = shaderStream.str();
	}
	else {
		throw new EngineException("Couldn't open shader file");
	}

	uint32_t includePosition = 0;
	string filePath(filename);
	
	uint32_t filePathPosition = filePath.find_last_of("/");
	filePath.erase(filePathPosition + 1, filePath.length() - 1);

	while ((includePosition = shaderCode.find("#include ")) != string::npos) {

		uint32_t lineBreakPosition = shaderCode.find("\n", includePosition);

		uint32_t filenamePosition = shaderCode.find_first_of("\"<", includePosition) + 1;
		uint32_t filenameEndPosition = shaderCode.find_first_of("\">", filenamePosition);

		string includeFilename = shaderCode.substr(filenamePosition, filenameEndPosition - filenamePosition);

		string includeCode = ReadShaderFile((filePath + includeFilename).c_str());

		shaderCode.replace(includePosition, lineBreakPosition, includeCode);
	
	}

	return shaderCode;

}