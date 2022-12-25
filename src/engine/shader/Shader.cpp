#include "Shader.h"
#include "../Log.h"

#include <algorithm>

namespace Atlas {

	namespace Shader {

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

            // glDeleteProgram(ID);

        }

        void Shader::AddStage(int32_t type, const std::string& filename) {

            auto source = new ShaderStage(type, filename);

            stages.push_back(source);

        }

        void Shader::AddStage(ShaderStage* stage) {

            stages.push_back(stage);

        }

        ShaderStage* Shader::GetStage(int32_t type) {

			auto it = std::find_if(stages.begin(), stages.end(),
				[type](const auto& stage) { return stage->type == type; });

			if (it != stages.end())
				return *it;

            return nullptr;

        }

        Uniform* Shader::GetUniform(const std::string& name) {

			// Check if we have the uniform cached
			auto it = std::find_if(uniforms.begin(), uniforms.end(),
				[name](const auto& uniform) { return uniform->name == name; });

			if (it != uniforms.end())
				return *it;
			
            Bind();

            Uniform* uniform = new Uniform(ID, name);

            uniforms.push_back(uniform);

            return uniform;

        }

        void Shader::AddMacro(const std::string& macro) {

            isCompiled = false;

            for (auto& stage : stages) {
                stage->AddMacro(macro);
            }

            macros.push_back(macro);

        }

        void Shader::RemoveMacro(const std::string& macro) {

            for (auto& stage : stages) {
                stage->RemoveMacro(macro);
            }

			auto it = std::find(macros.begin(), macros.end(), macro);

			if (it != macros.end()) {
				macros.erase(it);
				isCompiled = false;
			}

        }

        bool Shader::HasMacro(const std::string& macro) {

			return std::any_of(macros.begin(), macros.end(),
				[macro](const auto& value) { return value == macro; });

        }

        bool Shader::ManageMacro(const std::string& macro, bool enable) {

            bool hasMacro = HasMacro(macro);
            if (enable && !hasMacro) {
                AddMacro(macro);
            }
            if (!enable && hasMacro) {
                RemoveMacro(macro);
            }
            return hasMacro != enable;

        }

        bool Shader::Compile() {

            bool compile = true;

            for (auto& stage : stages) {
                compile = compile & stage->Compile();
            }

            if (compile) {

                /*
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

                    Log::Message("Compiled shader with ID " + std::to_string(ID));

                    Bind();

                    for (auto& uniform : uniforms) {
                        uniform->Update();
                    }

                    return true;

                }

                int32_t programLogLength, length;
                glGetProgramiv(ID, GL_INFO_LOG_LENGTH, &programLogLength);
                auto programLog = std::vector<char>(programLogLength);
                glGetProgramInfoLog(ID, programLogLength, &length, programLog.data());

                std::string log = "Error linking shader files:";
                for (auto& stage : stages) {
                    log.append(stage->filename + "\n");
					log.append(stage->GetErrorLog() + "\n");
                }

                if (programLog.size())
                    log.append(programLog.data());

				Log::Error(log);
                 */

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
#ifdef AE_INSTANT_SHADER_RELOAD
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
                // glUseProgram(ID);

                boundShaderID = ID;
            }

        }

        void Shader::Unbind() {

            // glUseProgram(0);
            boundShaderID = 0;

        }

        uint32_t Shader::GetID() {

            return ID;

        }

	}

}