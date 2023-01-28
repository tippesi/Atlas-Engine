#include "ShaderLoader.h"
#include "AssetLoader.h"

#include "graphics/Extensions.h"
#include "../common/Path.h"

namespace Atlas {

    namespace Loader {

        std::string ShaderLoader::sourceDirectory = "";

        Graphics::ShaderStageFile ShaderLoader::LoadFile(const std::string &filename, VkShaderStageFlagBits shaderStage) {

            auto path = sourceDirectory.length() != 0 ? sourceDirectory + "/" : "";
            path += filename;

            std::vector<std::string> includes;
            std::vector<Graphics::ShaderStageFile::Extension> extensions;
            auto code = ReadShaderFile(path, true, includes, extensions);

            Graphics::ShaderStageFile shaderStageFile;

            shaderStageFile.filename = filename;
            shaderStageFile.code = code;
            shaderStageFile.includes = includes;
            shaderStageFile.extensions = extensions;
            shaderStageFile.shaderStage = shaderStage;
            shaderStageFile.lastModified = std::filesystem::last_write_time(Loader::AssetLoader::GetFullPath(path));

            return shaderStageFile;

        }

        bool ShaderLoader::CheckForReload(const std::string& filename, const std::filesystem::file_time_type fileTime) {

            auto path = sourceDirectory.length() != 0 ? sourceDirectory + "/" : "";
            path += filename;

            return std::filesystem::last_write_time(Loader::AssetLoader::GetFullPath(path)) != fileTime;

        }

        void ShaderLoader::SetSourceDirectory(const std::string &directory) {

            sourceDirectory = directory;

        }

        std::string ShaderLoader::ReadShaderFile(const std::string& filename, bool mainFile,
            std::vector<std::string>& includes, std::vector<Graphics::ShaderStageFile::Extension>& extensions) {

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

            lines = ExtractExtensions(lines, extensions);

            for (auto& codeLine : lines) {
                shaderCode += codeLine + "\n\r";
            }

            shaderCode = ExtractIncludes(filename, shaderCode, includes, extensions);

            return shaderCode;

        }

        std::string ShaderLoader::ExtractIncludes(const std::string& filename, std::string& code,
            std::vector<std::string>& includes, std::vector<Graphics::ShaderStageFile::Extension>& extensions) {

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
                if (std::find(includes.begin(), includes.end(), includePath) == includes.end()) {

                    includes.push_back(includePath);
                    auto includeCode = ReadShaderFile(includePath, false, includes, extensions);

                    code = codeBeforeInclude + includeCode + codeAfterInclude;

                }
                else {

                    code = codeBeforeInclude + codeAfterInclude;

                }

            }

            return code;

        }

        std::vector<std::string> ShaderLoader::ExtractExtensions(std::vector<std::string> codeLines,
            std::vector<Graphics::ShaderStageFile::Extension>& extensions) {

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
                    commentPos = line.find("//");
                    comment = commentPos != std::string::npos;
                    multilineCommentPos = line.find("/*");
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

                    // If we have a valid directive, we don't want to include
                    // it in the lines of code
                    if (validExtension) continue;

                }

                lines.push_back(line);

            }

            return lines;

        }

    }

}