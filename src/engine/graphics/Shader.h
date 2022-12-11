#ifndef AE_GRAPHICSSHADER_H
#define AE_GRAPHICSSHADER_H

#include "Common.h"
#include "MemoryManager.h"

#include <vector>
#include <string>

namespace Atlas {

    namespace Graphics {

        class ShaderStageFile {
        public:
            ShaderStageFile() = default;

            const std::string GetGlslCode() const;

            struct Extension {
                std::string extension;
                std::vector<std::string> ifdefs;
            };

            std::string filename;
            std::string code;
            std::vector<std::string> includes;
            std::vector<std::string> macros;
            std::vector<Extension> extensions;

            VkShaderStageFlagBits shaderStage;
            std::vector<uint32_t> spirvBinary;
            bool isCompiled = false;

        };

        struct ShaderDesc {
            std::vector<ShaderStageFile> stages;
            VkRenderPass renderPass;
            uint32_t viewportWidth;
            uint32_t viewportHeight;
        };

        class Shader {

        public:
            Shader(MemoryManager* memManager, ShaderDesc& shaderDesc);

            ~Shader();

            VkPipeline pipeline;
            VkPipelineLayout pipelineLayout;

            bool isComplete = false;
            bool isCompute = true;

        private:
            std::vector<VkShaderModule> shaderModules;
            MemoryManager* memoryManager = nullptr;

        };

    }

}

#endif
