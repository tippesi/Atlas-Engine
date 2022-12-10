#ifndef AE_GRAPHICSSHADER_H
#define AE_GRAPHICSSHADER_H

#include "Common.h"

#include <vector>
#include <string>

namespace Atlas {

    namespace Graphics {

        class ShaderStageFile {
        public:
            ShaderStageFile() = default;

            const std::string GetGlslCode(std::vector<std::string> macros = std::vector<std::string>()) const;

            void Compile();

            struct Extension {
                std::string extension;
                std::vector<std::string> ifdefs;
            };

            std::string filename;
            std::string code;
            std::vector<std::string> includes;
            std::vector<Extension> extensions;

            VkShaderStageFlagBits shaderStage;
            std::vector<uint32_t> spirvBinary;
            bool compiled = false;

        };

        struct ShaderDesc {
            ShaderStageFile vertexShaderStage;
            ShaderStageFile fragmentShaderStage;
        };

        class Shader {

        };

    }

}

#endif
