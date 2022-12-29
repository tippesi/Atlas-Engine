#ifndef AE_GRAPHICSSHADER_H
#define AE_GRAPHICSSHADER_H

#include "Common.h"
#include <vector>
#include <string>
#include <mutex>

namespace Atlas {

    namespace Graphics {

        class GraphicsDevice;

        class ShaderStageFile {
        public:
            ShaderStageFile() = default;

            const std::string GetGlslCode(const std::vector<std::string>& macros) const;

            struct Extension {
                std::string extension;
                std::vector<std::string> ifdefs;
            };

            std::string filename;
            std::string code;
            std::vector<std::string> includes;
            std::vector<Extension> extensions;

            VkShaderStageFlagBits shaderStage;

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

        class ShaderVariant {
        public:
            ShaderVariant(GraphicsDevice* device, std::vector<ShaderStageFile>& stages,
                const std::vector<std::string>& macros);

            ~ShaderVariant();

            PushConstantRange* GetPushConstantRange(const std::string& name);

            std::vector<VkShaderModule> modules;
            std::vector<VkPipelineShaderStageCreateInfo> stageCreateInfos;

            std::vector<PushConstantRange> pushConstantRanges;
            ShaderDescriptorSet sets[DESCRIPTOR_SET_COUNT];

            std::vector<std::string> macros;

            bool isComplete = false;
            bool isCompute = true;

        private:
            // Temporary data structure
            struct ShaderModule {
                VkShaderStageFlagBits shaderStageFlag = {};

                std::vector<PushConstantRange> pushConstantRanges;
                std::vector<ShaderDescriptorSet> sets;
            };

            void GenerateReflectionData(ShaderModule& shaderModule, const std::vector<uint32_t>& spirvBinary);

            GraphicsDevice* device = nullptr;

        };

        class Shader {

        public:
            Shader(GraphicsDevice* device, ShaderDesc& desc);

            ~Shader();

            Ref<ShaderVariant> GetVariant();

            Ref<ShaderVariant> GetVariant(std::vector<std::string> macros);

        private:
            Ref<ShaderVariant> FindVariant(const std::vector<std::string>& macros);

            GraphicsDevice* device = nullptr;

            std::vector<ShaderStageFile> shaderStageFiles;

            std::mutex variantMutex;
            std::vector<Ref<ShaderVariant>> shaderVariants;

        };

    }

}

#endif
