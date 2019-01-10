#include "Shader.h"

uint32_t Shader::boundShaderID = 0;

Shader::Shader() {

	ID = 0;
	isCompiled = false;

}

Shader::~Shader() {

	for (auto& stage : stages) {
		delete stage;
	}

	for (auto& uniform : uniforms) {
		delete uniform;
	}

	glDeleteProgram(ID);

}

void Shader::AddStage(int32_t type, string filename) {

	auto source = new ShaderStage(type, filename);

	stages.push_back(source);

}

void Shader::AddStage(ShaderStage* source) {

	stages.push_back(source);

}

ShaderStage* Shader::GetStage(int32_t type) {

	for (auto& stage : stages) {
		if (stage->type == type) {
			return stage;
		}
	}

	return nullptr;

}

Uniform* Shader::GetUniform(string uniformName) {

	Bind();

	Uniform* uniform = new Uniform(ID, uniformName);

	uniforms.push_back(uniform);

	return uniform;

}

void Shader::AddMacro(string macro) {

	isCompiled = false;

	for (auto& stage : stages) {
		stage->AddMacro(macro);
	}

	macros.push_back(macro);

}

void Shader::RemoveMacro(string macro) {

	for (auto& stage : stages) {
		stage->RemoveMacro(macro);
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

	for (auto& compMacro : macros) {
		if (compMacro == macro) {
			return true;
		}
	}

	return false;

}

bool Shader::Compile() {

	bool compile = true;

	for (auto& stage : stages) {
		compile = compile & stage->Compile();
	}

	if (compile) {

		// We only want to create a program once
		if (ID == 0) {
			ID = glCreateProgram();
		}

		for (auto& stage : stages) {
			glAttachShader(ID, stage->ID);
		}

		glLinkProgram(ID);

		for (auto& stage : stages) {
			glDetachShader(ID, stage->ID);
		}

		int32_t isLinked = 0;
		glGetProgramiv(ID, GL_LINK_STATUS, &isLinked);

		if (isLinked) {

			isCompiled = true;

#ifdef ENGINE_SHOW_LOG
			EngineLog("Compiled shader with ID %d", ID);
#endif
			
			Bind();

			for (auto& uniform : uniforms) {
				uniform->Update();
			}
		
			return true;

		}

#ifdef ENGINE_SHOW_LOG
		int32_t programLogLength, length;
		glGetProgramiv(ID, GL_INFO_LOG_LENGTH, &programLogLength);
		auto programLog = vector<char>(programLogLength);
		glGetProgramInfoLog(ID, programLogLength, &length, programLog.data());

		EngineLog("Error linking shader files:");
		for (auto& stage : stages) {
			EngineLog("%s", stage->filename.c_str());
		}
		EngineLog("%s", programLog.data());
#endif

	}

	ID = 0;

	return false;

}

bool Shader::IsCompiled() {

	return isCompiled;

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
	for (auto& stage : stages) {
		reloaded = stage->Reload() ? true : reloaded;
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

void Shader::Unbind() {

	glUseProgram(0);
	boundShaderID = 0;

}

uint32_t Shader::GetID() {

	return ID;

}