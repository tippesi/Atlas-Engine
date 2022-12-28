#ifndef AE_GRAPHICSSHADER_H
#define AE_GRAPHICSSHADER_H

#include "Common.h"
#include <vector>
#include <string>

namespace Atlas {

    namespace Graphics {

        class GraphicsDevice;

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

        struct ShaderDescriptorBinding {
            std::string name;

            uint32_t set = 0;
            uint32_t size = 0;
            uint32_t arrayElement = 0;
            VkDescriptorSetLayoutBinding layoutBinding = {};
        };

        struct ShaderDescriptorSet {
            ShaderDescriptorBinding bindings[BINDINGS_PER_DESCRIPTOR_SET];
            uint32_t bindingCount = 0;

            VkDescriptorSetLayout layout = {};
            VkDescriptorSetLayoutBinding layoutBindings[BINDINGS_PER_DESCRIPTOR_SET];
        };

        struct ShaderModule {
            VkShaderModule module;
            VkShaderStageFlagBits shaderStageFlag = {};

            std::vector<PushConstantRange> pushConstantRanges;
            std::vector<ShaderDescriptorSet> sets;
        };

        class Shader {

        public:
            Shader(GraphicsDevice* device, ShaderDesc& shaderDesc);

            ~Shader();

            PushConstantRange* GetPushConstantRange(const std::string& name);

            std::vector<ShaderModule> shaderModules;
            std::vector<VkPipelineShaderStageCreateInfo> shaderStageCreateInfos;

            std::vector<PushConstantRange> pushConstantRanges;
            ShaderDescriptorSet sets[DESCRIPTOR_SET_COUNT];

            bool isComplete = false;
            bool isCompute = true;

        private:
            void GenerateReflectionData(ShaderModule& shaderModule, ShaderStageFile& shaderStageFile);

            bool CheckPushConstantsStageCompatibility();

            GraphicsDevice* device = nullptr;

        };

    }

}

#endif
