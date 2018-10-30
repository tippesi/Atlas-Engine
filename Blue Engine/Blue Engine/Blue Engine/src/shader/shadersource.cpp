#include "shadersource.h"
#include <fstream>
#include <sstream>

ShaderSource::ShaderSource(int32_t type, const char* filename) : type(type), filename(filename) {

	code = ReadShaderFile(filename, true);
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

ShaderConstant* ShaderSource::GetConstant(const char* constant) {

	string constantString(constant);

	for (list<ShaderConstant*>::iterator iterator = constants.begin(); iterator != constants.end(); iterator++) {
		if ((*iterator)->GetName() == constantString) {
			return *iterator;
		}
	}

	return nullptr;

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

	for (list<ShaderConstant*>::iterator iterator = constants.begin(); iterator != constants.end(); iterator++) {
		composedCode.append((*iterator)->GetValuedString() + "\n");
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

ShaderSource::~ShaderSource() {

	glDeleteShader(ID);

}

string ShaderSource::ReadShaderFile(const char* filename, bool mainFile) {

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

	string filePath(filename);
	
	uint32_t filePathPosition = filePath.find_last_of("/");
	filePath.erase(filePathPosition + 1, filePath.length() - 1);

	// Copy all includes into the code
	while (shaderCode.find("#include ") != string::npos) {

		uint32_t includePosition = shaderCode.find("#include ");
		uint32_t lineBreakPosition = shaderCode.find("\n", includePosition);

		uint32_t filenamePosition = shaderCode.find_first_of("\"<", includePosition) + 1;
		uint32_t filenameEndPosition = shaderCode.find_first_of("\">", filenamePosition);

		string includeFilename = shaderCode.substr(filenamePosition, filenameEndPosition - filenamePosition);

		string includeCode = ReadShaderFile((filePath + includeFilename).c_str(), false);

		shaderCode.replace(includePosition, lineBreakPosition, includeCode);
	
	}

	// Find constants in the code (we have to consider that we don't want to change the constants in functions)	
	if (mainFile) {

		int32_t openedCurlyBrackets = 0;

		for (uint32_t i = 0; i < shaderCode.length(); i++) {
			if (shaderCode[i] == '{') {
				openedCurlyBrackets++;
			}
			else if (shaderCode[i] == '}') {
				openedCurlyBrackets--;
			}
			else if (shaderCode[i] == 'c' && openedCurlyBrackets == 0) {
				// Check if its a constant
				int32_t position = shaderCode.find("const ", i);
				if (position == i) {
					// Create a new constant
					int32_t constantEndPosition = shaderCode.find(";", i);
					string constantString = shaderCode.substr(i, constantEndPosition - i + 1);
					ShaderConstant* constant = new ShaderConstant(constantString.c_str());
					constants.push_back(constant);
					// Remove the constant expression from the code and reduce i
					shaderCode.erase(i, constantEndPosition - i + 1);
					i--;
				}
			}
		}

	}
	
	return shaderCode;

}