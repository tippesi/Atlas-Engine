#include "ShaderSource.h"
#include <fstream>
#include <sstream>

string ShaderSource::sourceDirectory = "";

ShaderSource::ShaderSource(int32_t type, const char* filename) : type(type), filename(filename) {

	string path = sourceDirectory.length() != 0 ? sourceDirectory + "/" : "";
	path += string(filename);

	code = ReadShaderFile(path.c_str(), true);
	lastModified = GetLastModified();

	ID = glCreateShader(type);

#ifdef ENGINE_SHOW_LOG
	EngineLog("Loaded shader file %s", filename);
#endif
}

ShaderSource::ShaderSource(ShaderSource* source) {

	code = source->code;
	macros = source->macros;
	filename = source->filename;
	lastModified = source->lastModified;

	for (list<ShaderConstant*>::iterator iterator = source->constants.begin(); iterator != source->constants.end(); iterator++) {
		ShaderConstant* constant = new ShaderConstant((*iterator)->GetValuedString().c_str());
		constants.push_back(constant);
	}

	ID = glCreateShader(source->type);

}

bool ShaderSource::Reload() {

	time_t comp = GetLastModified();

	if (comp == lastModified) {
		return false;
	}

	lastModified = comp;

	string path = sourceDirectory.length() != 0 ? sourceDirectory + "/" : "";
	path += string(filename);

	constants.clear();
	code = ReadShaderFile(path.c_str(), true);

	return true;

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
	composedCode.append("#version 330 core\n\n#define ENGINE_OGL\n");
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

void ShaderSource::SetSourceDirectory(const char* directory) {

	sourceDirectory = string(directory);

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
#ifdef ENGINE_SHOW_LOG
		EngineLog("Shader file %s not found", filename);
#endif
		throw new EngineException("Couldn't open shader file");
	}

	string filePath(filename);
	
	size_t filePathPosition = filePath.find_last_of("/");
	filePath.erase(filePathPosition + 1, filePath.length() - 1);

	// Copy all includes into the code
	while (shaderCode.find("#include ") != string::npos) {

		size_t includePosition = shaderCode.find("#include ");
		size_t lineBreakPosition = shaderCode.find("\n", includePosition);

		size_t filenamePosition = shaderCode.find_first_of("\"<", includePosition) + 1;
		size_t filenameEndPosition = shaderCode.find_first_of("\">", filenamePosition);

		string includeFilename = shaderCode.substr(filenamePosition, filenameEndPosition - filenamePosition);

		string includeCode = ReadShaderFile((filePath + includeFilename).c_str(), false);

		string codeBeforeInclude = shaderCode.substr(0, includePosition);
		string codeAfterInclude = shaderCode.substr(lineBreakPosition, shaderCode.length() - 1);

		shaderCode = codeBeforeInclude + includeCode + codeAfterInclude;
	
	}

	// Find constants in the code (we have to consider that we don't want to change the constants in functions)	
	if (mainFile) {

		int32_t openedCurlyBrackets = 0;

		for (size_t i = 0; i < shaderCode.length(); i++) {
			if (shaderCode[i] == '{') {
				openedCurlyBrackets++;
			}
			else if (shaderCode[i] == '}') {
				openedCurlyBrackets--;
			}
			else if (shaderCode[i] == 'c' && openedCurlyBrackets == 0) {
				// Check if its a constant
				size_t position = shaderCode.find("const ", i);
				if (position == i) {
					// Create a new constant
					size_t constantEndPosition = shaderCode.find(";", i);
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

time_t ShaderSource::GetLastModified() {

	string path = sourceDirectory.length() != 0 ? sourceDirectory + "/" : "";
	path += string(filename);

	struct stat result;
	if (stat(path.c_str(), &result) == 0) {
		auto mod_time = result.st_mtime;
		return mod_time;
	}
	return 0;

}