#include "Shader.h"

uint32_t Shader::boundShaderID = 0;

Shader::Shader() {

	ID = 0;
	isCompiled = false;

}

void Shader::AddComponent(int32_t type, string filename) {

	ShaderSource* source = new ShaderSource(type, filename);

	components.push_back(source);

}

ShaderSource* Shader::GetComponent(int32_t type) {

	for (ShaderSource*& source : components) {
		if (source->type == type) {
			return source;
		}
	}

	return nullptr;

}

void Shader::AddComponent(ShaderSource* source) {

	components.push_back(source);

}

Uniform* Shader::GetUniform(string uniformName) {

	Bind();

	Uniform* uniform = new Uniform(ID, uniformName);

	uniforms.push_back(uniform);

	return uniform;

}

void Shader::AddMacro(string macro) {

	isCompiled = false;

	for (ShaderSource* source : components) {
		source->AddMacro(macro);
	}

	macros.push_back(macro);

}

void Shader::RemoveMacro(string macro) {

	for (ShaderSource* source : components) {
		source->RemoveMacro(macro);
	}

	for (auto iterator = macros.begin(); iterator != macros.end(); iterator++) {
		if (macro == *iterator) {
			macros.erase(iterator);
			isCompiled = false;
			return;
		}
	}

}

bool Shader::HasMacro(string macro) {

	for (string compMacro : macros) {
		if (compMacro == macro) {
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

		for (auto& source : components) {
			glAttachShader(ID, source->ID);
		}

		glLinkProgram(ID);

		for (auto& source : components) {
			glDetachShader(ID, source->ID);
		}

		int32_t isLinked = 0;
		glGetProgramiv(ID, GL_LINK_STATUS, &isLinked);

		if (isLinked) {

			isCompiled = true;

#ifdef ENGINE_SHOW_LOG
			EngineLog("Compiled shader with ID %d", ID);
#endif
			
			Bind();

			for (Uniform*& uniform : uniforms) {
				uniform->Update();
			}
		
			return true;

		}

#ifdef ENGINE_SHOW_LOG
		int32_t programLogLength, length;
		glGetProgramiv(ID, GL_INFO_LOG_LENGTH, &programLogLength);
		char *programLog = new char[programLogLength];
		glGetProgramInfoLog(ID, programLogLength, &length, programLog);

		EngineLog("Error linking shader files:");
		for (auto& source : components) {
			EngineLog("%s", source->filename.c_str());
		}
		EngineLog("%s", programLog);
		delete[] programLog;
#endif

	}

	ID = 0;

	return false;

}

void Shader::Bind() {

	if (!isCompiled) {
		Compile();
		if (!isCompiled) {
			return;
		}
	}
#ifdef ENGINE_INSTANT_SHADER_RELOAD
	bool reloaded = false;
	for (ShaderSource*& source : components) {
		reloaded = source->Reload() ? true : reloaded;
	}
	if (reloaded) {
		Compile();
		if (!isCompiled) {
			return;
		}
	}
#endif

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