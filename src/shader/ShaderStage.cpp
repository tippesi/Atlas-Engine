#include "ShaderStage.h"
#include "../loader/AssetLoader.h"
#include "../common/Path.h"
#include "../Log.h"
#include "../Extensions.h"

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
            extensions.clear();
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

            composedCode.append("#version 430\n\n#define AE_API_GL\n");

            if (Extensions::IsSupported("GL_EXT_texture_shadow_lod")) {
                composedCode.append("#define AE_TEXTURE_SHADOW_LOD\n");
            }

            for (auto& macro : macros) {
                composedCode.append("#define " + macro + "\n");
            }

            for (auto& extension : extensions) {
                for (auto& ifdef : extension.ifdefs)
                    composedCode += ifdef + "\n";
                composedCode += extension.extension + "\n";
                for (size_t i = 0; i < extension.ifdefs.size(); i++)
                    composedCode += "#endif\n";
            }

            for (auto& constant : constants) {
                composedCode.append(constant->GetValuedString() + "\n");
            }

            composedCode.append(code);
            composedCode.erase(std::remove(composedCode.begin(), composedCode.end(), '\r'), composedCode.end());

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

            int32_t lineCount = 1;
            size_t pos = 0, lastPos = 0;

            while ((pos = stageCode.find('\n', lastPos)) != std::string::npos) {
                log.append("[" + std::to_string(lineCount++) + "] ");
                log.append(stageCode.substr(lastPos, pos - lastPos + 1));
                lastPos = pos + 1;
            }

            log.append("\nFile: " + filename);
            log.append("\nError: " + std::string(shaderLog.data()));

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
            
            std::string line;
            std::vector<std::string> lines;
            while (std::getline(shaderStream, line)) lines.push_back(line);

            shaderFile.close();

            lines = ExtractExtensions(lines);

            for (auto& line : lines) {
                shaderCode += line + "\n\r";
            }

            shaderCode = ExtractIncludes(filename, shaderCode);

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

        std::string ShaderStage::ExtractIncludes(const std::string& filename, std::string& code) {

            auto directory = Common::Path::GetDirectory(filename) + "/";

            // Copy all includes into the code
            while (code.find("#include ") != std::string::npos) {

                size_t includePosition = code.find("#include ");
                size_t lineBreakPosition = code.find("\n", includePosition);

                size_t filenamePosition = code.find_first_of("\"<", includePosition) + 1;
                size_t filenameEndPosition = code.find_first_of("\">", filenamePosition);

                auto includeFilename = code.substr(filenamePosition, filenameEndPosition - filenamePosition);
                auto includePath = directory + includeFilename;
                
                includePath = Loader::AssetLoader::GetFullPath(includePath);
                includePath = Common::Path::Normalize(includePath);

                auto codeBeforeInclude = code.substr(0, includePosition);
                auto codeAfterInclude = code.substr(lineBreakPosition, code.length() - 1);

                // Check for multiple includes of a file
                if (includes.find(includePath) == includes.end()) {

                    includes.insert(includePath);
                    auto includeCode = ReadShaderFile(includePath, false);

                    code = codeBeforeInclude + includeCode + codeAfterInclude;

                }
                else {

                    code = codeBeforeInclude + codeAfterInclude;

                }

            }

            return code;

        }

        std::vector<std::string> ShaderStage::ExtractExtensions(std::vector<std::string> codeLines) {

            std::vector<std::string> lines;
            std::vector<std::string> ifdefs;

            size_t continuousBuildup = 0;
            bool multilineComment = false;
            auto multilineCommentPos = 0;
            // Extract extensions from shader code
            for (auto& line : codeLines) {
                auto comment = false;
                auto commentPos = 0;
                if (!multilineComment) {
                    commentPos = line.find_first_of("//");
                    comment = commentPos != std::string::npos;
                    multilineCommentPos = line.find_first_of("/*");
                    multilineComment = multilineCommentPos != std::string::npos;
                }

                // A multiline comment close needs to be on it's own line
                // or on the same line shouldn't be an directive
                if (multilineComment) {
                    if (line.find("*/") != std::string::npos) {
                        multilineComment = false;
                        multilineCommentPos = 0;
                    }
                }
                else {

                    bool validExtension = false;
                    // Note: We will only allow one directive per line
                    // Doesn't really support #else right now
                    auto directives = 0;
                    auto directivePos = line.find("#ifdef ");
                    if (directivePos != std::string::npos &&
                        (!comment || directivePos < commentPos)) {
                        directives++;
                        continuousBuildup++;
                    }

                    directivePos = line.find("#if ");
                    if (directivePos != std::string::npos &&
                        (!comment || directivePos < commentPos)) {
                        directives++;
                        continuousBuildup++;
                    }

                    directivePos = line.find("#ifndef ");
                    if (directivePos != std::string::npos &&
                        (!comment || directivePos < commentPos)) {
                        directives++;
                        continuousBuildup++;
                    }

                    if (directives > 0) ifdefs.push_back(line);

                    directivePos = line.find("#endif");
                    if (directivePos != std::string::npos &&
                        (!comment || directivePos < commentPos)) {
                        directives++;
                        continuousBuildup++;
                        ifdefs.pop_back();
                    }

                    directivePos = line.find("#extension ");
                    if (directivePos != std::string::npos &&
                        (!comment || directivePos < commentPos)) {
                        directives++;
                        validExtension = true;
                        extensions.push_back({ line, ifdefs });
                    }

                    if (directives > 1) continuousBuildup++;

                    auto emptyLine = line.find_first_not_of(" \n\r\t") == std::string::npos;
                    if (directives == 0 && emptyLine) continuousBuildup++;
                    if (directives == 0 && !emptyLine) {
                        // Remove all directive lines after a continous buildup of if(defs)
                        // Needs to have at least 3 lines to complete a valid buildup
                        if (!ifdefs.size() && continuousBuildup >= 3) {
                            for (size_t i = 0; i < continuousBuildup; i++)
                                lines.pop_back();
                        }
                        continuousBuildup = 0;
                    }

                    // If we have a valid directive, we don't want to include
                    // it in the lines of code
                    if (validExtension) continue;

                }

                lines.push_back(line);

            }

            return lines;

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
            extensions = that.extensions;
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