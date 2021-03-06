#include "ShaderStage.h"
#include "../loader/AssetLoader.h"
#include "../common/Path.h"
#include "../Log.h"

#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <vector>
#include <algorithm>

namespace Atlas {

    namespace Shader {

        std::string ShaderStage::sourceDirectory = "";

        ShaderStage::ShaderStage(int32_t type, const std::string& filename) : type(type), filename(filename) {

            auto path = sourceDirectory.length() != 0 ? sourceDirectory + "/" : "";
            path += filename;

            code = ReadShaderFile(path, true);

            lastModified = GetLastModified();

            ID = glCreateShader(type);

            Log::Message("Loaded shader file " + filename);

        }

        ShaderStage::ShaderStage(const ShaderStage& that) {

            DeepCopy(that);

        }

        ShaderStage::~ShaderStage() {

            for (auto& constant : constants) {
                delete constant;
            }

            glDeleteShader(ID);

        }

        bool ShaderStage::Reload() {

            time_t comp = GetLastModified();

            if (comp == lastModified || !filename.length()) {
                return false;
            }
			
			error = false;

            lastModified = comp;

            auto path = sourceDirectory.length() != 0 ? sourceDirectory + "/" : "";
            path += filename;

            constants.clear();
			includes.clear();
            code = ReadShaderFile(path, true);

            return true;

        }

        void ShaderStage::AddMacro(const std::string& macro) {

            macros.push_back(macro);

        }

        void ShaderStage::RemoveMacro(const std::string& macro) {

            auto item = std::find(macros.begin(), macros.end(), macro);

            if (item != macros.end()) {
                macros.erase(item);
            }

        }

        ShaderConstant* ShaderStage::GetConstant(const std::string& name) {

            auto constant = std::find_if(constants.begin(), constants.end(), 
                [name](ShaderConstant* constant) { return constant->GetName() == name; });

            if (constant != constants.end())
                return *constant;

            return nullptr;

        }

        bool ShaderStage::Compile() {

            std::string composedCode;

#ifdef AE_API_GL
            composedCode.append("#version 430\n\n#define AE_API_GL\n");
#elif AE_API_GLES
            composedCode.append("#version 320 es\n\nprecision highp float;\nprecision highp sampler2D;\
				\nprecision highp usampler2D;\nprecision highp image2D;\nprecision highp image2DArray;\
				\nprecision highp samplerCube;\nprecision highp sampler2DArrayShadow;\nprecision highp sampler2DArray;\
				\nprecision highp samplerCubeShadow;\nprecision highp imageCube;\nprecision highp int;\n#define AE_API_GLES\n");
#endif

            for (auto& macro : macros) {
                composedCode.append("#define " + macro + "\n");
            }

            for (auto& constant : constants) {
                composedCode.append(constant->GetValuedString() + "\n");
            }

            composedCode.append(code);

			stageCode = composedCode;

            const char* convertedCode = composedCode.c_str();

            int compiled = 0;

            glShaderSource(ID, 1, &convertedCode, 0);
            glCompileShader(ID);
            glGetShaderiv(ID, GL_COMPILE_STATUS, &compiled);

            if (!compiled && !error) {

				error = true;

				Log::Error(GetErrorLog());

                return false;

            }

            return true;

        }

        void ShaderStage::SetSourceDirectory(const std::string& directory) {

            sourceDirectory = directory;

        }

        std::string ShaderStage::GetErrorLog() {

            std::string log;

            int32_t shaderLogLength, length;
            glGetShaderiv(ID, GL_INFO_LOG_LENGTH, &shaderLogLength);

            if (!shaderLogLength)
                return "No error";

            auto shaderLog = std::vector<char>(shaderLogLength);
            glGetShaderInfoLog(ID, shaderLogLength, &length, shaderLog.data());

            if (type == AE_VERTEX_STAGE) {
                log.append("Compiling vertex stage failed.");
            }
            else if (type == AE_FRAGMENT_STAGE) {
                log.append("Compiling fragment stage failed.");
            }
            else if (type == AE_GEOMETRY_STAGE) {
                log.append("Compiling geometry stage failed.");
            }
            else if (type == AE_TESSELLATION_CONTROL_STAGE) {
                log.append("Compiling tessellation control stage failed.");
            }
            else if (type == AE_TESSELLATION_EVALUATION_STAGE) {
                log.append("Compiling tessellation evaluation stage failed.");
            }
            else if (type == AE_COMPUTE_STAGE) {
                log.append("Compiling compute stage failed.");
            }

            log.append("\nFile: " + filename);
            log.append("\nError: " + std::string(shaderLog.data()));

            int32_t lineCount = 1;
            size_t pos = 0, lastPos = 0;

            while ((pos = stageCode.find('\n', lastPos)) != std::string::npos) {
                log.append("[" + std::to_string(lineCount++) + "] ");
                log.append(stageCode.substr(lastPos, pos - lastPos + 1));
                lastPos = pos + 1;
            }

            return log;

        }

        std::string ShaderStage::ReadShaderFile(const std::string& filename, bool mainFile) {

            std::string shaderCode;
            std::ifstream shaderFile;
            std::stringstream shaderStream;

            shaderFile = Loader::AssetLoader::ReadFile(filename, std::ios::in);

            if (!shaderFile.is_open()) {
				Log::Error("Shader file not found " + filename);
				return "";
            }

            shaderStream << shaderFile.rdbuf();
            shaderFile.close();
            shaderCode = shaderStream.str();

			auto directory = Common::Path::GetDirectory(filename) + "/";

            // Copy all includes into the code
            while (shaderCode.find("#include ") != std::string::npos) {

                size_t includePosition = shaderCode.find("#include ");
                size_t lineBreakPosition = shaderCode.find("\n", includePosition);

                size_t filenamePosition = shaderCode.find_first_of("\"<", includePosition) + 1;
                size_t filenameEndPosition = shaderCode.find_first_of("\">", filenamePosition);

                auto includeFilename = shaderCode.substr(filenamePosition, filenameEndPosition - filenamePosition);

				auto shortenedFilename = Common::Path::GetFileName(includeFilename);

				auto codeBeforeInclude = shaderCode.substr(0, includePosition);
				auto codeAfterInclude = shaderCode.substr(lineBreakPosition, shaderCode.length() - 1);

				// Check for multiple includes of a file
				if (includes.find(shortenedFilename) == includes.end()) {

					includes.insert(shortenedFilename);
					auto includeCode = ReadShaderFile(directory + includeFilename, false);

					shaderCode = codeBeforeInclude + includeCode + codeAfterInclude;

				}
				else {

					shaderCode = codeBeforeInclude + codeAfterInclude;

				}

            }

            // Find constants in the code (we have to consider that we don't 
			//want to change the constants in functions or in function definitions)
            if (mainFile) {

                int32_t openedCurlyBrackets = 0;
				int32_t openedBrackets = 0;

                for (size_t i = 0; i < shaderCode.length(); i++) {
                    if (shaderCode[i] == '{') {
                        openedCurlyBrackets++;
                    }
                    else if (shaderCode[i] == '}') {
                        openedCurlyBrackets--;
                    }
					else if (shaderCode[i] == '(') {
						openedBrackets++;
					}
					else if (shaderCode[i] == ')') {
						openedBrackets--;
					}
                    else if (shaderCode[i] == 'c' && !openedCurlyBrackets && !openedBrackets) {
                        // Check if its a constant
                        size_t position = shaderCode.find("const ", i);
                        if (position == i) {
                            // Create a new constant
                            size_t constantEndPosition = shaderCode.find(";", i);
                            auto constantString = shaderCode.substr(i, constantEndPosition - i + 1);
                            ShaderConstant* constant = new ShaderConstant(constantString);
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

        time_t ShaderStage::GetLastModified() {

            auto path = sourceDirectory.length() != 0 ? sourceDirectory + "/" : "";
            path += filename;

			path = Loader::AssetLoader::GetFullPath(path);

            struct stat result;
            if (stat(path.c_str(), &result) == 0) {
                auto mod_time = result.st_mtime;
                return mod_time;
            }
            return 0;

        }

        void ShaderStage::DeepCopy(const ShaderStage& that) {

            type = that.type;
            code = that.code;
            macros = that.macros;
            includes = that.includes;
            filename = that.filename;
            lastModified = that.lastModified;

#ifdef AE_SHOW_LOG
            stageCode = that.stageCode;
#endif

            for (auto iterator = that.constants.begin(); iterator != that.constants.end(); iterator++) {
                auto constant = new ShaderConstant((*iterator)->GetValuedString().c_str());
                constants.push_back(constant);
            }

            ID = glCreateShader(that.type);

        }

    }

}