#include "shader.h"

uint32_t Shader::boundShaderID = 0;

Shader::Shader() {

	ID = 0;
	isCompiled = false;

}

ShaderSource* Shader::AddComponent(int32_t type, const char* filename) {

	ShaderSource* source = new ShaderSource(type, filename);

	components.push_back(source);

	return source;

}

void Shader::AddComponent(ShaderSource* source) {

	components.push_back(source);

}

Uniform* Shader::GetUniform(const char* uniformName) {

	if (!isCompiled) {
		if (!Compile()) {
			return NULL;
		}
	}

	Bind();

	return new Uniform(ID, uniformName);

}

void Shader::AddMacro(const char* macro) {

	for (ShaderSource* source : components) {
		source->AddMacro(macro);
	}

	macros.push_back(string(macro));

}

void Shader::RemoveMacro(const char* macro) {

	for (ShaderSource* source : components) {
		source->RemoveMacro(macro);
	}

	string macroString(macro);

	for (auto iterator = macros.begin(); iterator != macros.end(); iterator++) {
		if (macroString == *iterator) {
			macros.erase(iterator);
			return;
		}
	}

}

bool Shader::HasMacro(const char* macro) {

	string macroString(macro);

	for (string macro : macros) {
		if (macro == macroString) {
			return true;
		}
	}

	return false;

}

bool Shader::Compile() {

	bool compile = true;

	for (ShaderSource* source : components) {
		compile = compile & source->Compile();
	}

	if (compile) {

		// We only want to create a program once
		if (ID == 0) {
			ID = glCreateProgram();
		}

		for (ShaderSource* source : components) {
			glAttachShader(ID, source->ID);
		}

		glLinkProgram(ID);

		for (ShaderSource* source : components) {
			glDetachShader(ID, source->ID);
		}

		int32_t isLinked = 0;
		glGetProgramiv(ID, GL_LINK_STATUS, &isLinked);

		if (isLinked) {

			isCompiled = true;

#ifdef ENGINE_SHOW_LOG
			EngineLog("Compiled shader with ID %d", ID);
#endif

			return true;

		}

	}

	ID = 0;

	return false;

}

void Shader::Bind() {

	if (boundShaderID != ID) {
		glUseProgram(ID);

		boundShaderID = ID;
	}

}

Shader::~Shader() {

	glDeleteProgram(ID);

	for (ShaderSource* source : components) {
		delete source;
	}

}