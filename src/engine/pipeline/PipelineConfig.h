#ifndef AE_PIPELINECONFIG_H
#define AE_PIPELINECONFIG_H

#include "../graphics/Pipeline.h"

#include <string>
#include <vector>
#include <utility>

namespace Atlas {

    using ShaderConfig = std::vector<std::pair<std::string, VkShaderStageFlagBits>>;

    class PipelineManager;

    class PipelineConfig {

        friend PipelineManager;

    public:
        PipelineConfig() = default;

        explicit PipelineConfig(const std::string& shaderFile);

        PipelineConfig(const std::string& shaderFile, const std::vector<std::string>& macros);

        PipelineConfig(const ShaderConfig& shaderConfig, const Graphics::GraphicsPipelineDesc& desc);

        PipelineConfig(const ShaderConfig& shaderConfig, const Graphics::GraphicsPipelineDesc& desc,
            const std::vector<std::string>& macros);

        void AddMacro(const std::string& macro);

        void RemoveMacro(const std::string& macro);

        bool HasMacro(const std::string& macro);

        bool ManageMacro(const std::string& macro, bool enable);

        bool IsValid() const;

        size_t shaderHash = 0;
        size_t variantHash = 0;

    private:
        void CalculateShaderHash();
        void CalculateVariantHash();

        ShaderConfig shaderConfig;
        std::vector<std::string> macros;

        bool isCompute = false;

        Graphics::GraphicsPipelineDesc graphicsPipelineDesc;
    };

}

#endif