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
        };

        struct PushConstantRange {
            std::string name;
            VkPushConstantRange range = {};
        };

        struct ShaderModule {
            VkShaderModule module;
            VkShaderStageFlagBits shaderStageFlag;
            std::vector<PushConstantRange> pushConstantRanges;
        };

        class Shader {

        public:
            Shader(MemoryManager* memManager, ShaderDesc& shaderDesc);

            ~Shader();

            PushConstantRange* GetPushConstantRange(const std::string& name);

            std::vector<ShaderModule> shaderModules;
            std::vector<VkPipelineShaderStageCreateInfo> shaderStageCreateInfos;

            std::vector<PushConstantRange> pushConstantRanges;

            bool isComplete = false;
            bool isCompute = true;

        private:
            void GenerateReflectionData(ShaderModule& shaderModule, ShaderStageFile& shaderStageFile);

            bool CheckPushConstantsStageCompatibility();

            MemoryManager* memoryManager = nullptr;

        };

    }

}

#endif
