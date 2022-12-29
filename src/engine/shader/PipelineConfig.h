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
        PipelineConfig(ShaderConfig shaderConfig, Graphics::GraphicsPipelineDesc desc);

        PipelineConfig(ShaderConfig shaderConfig, Graphics::GraphicsPipelineDesc desc, std::vector<std::string> macros);

        PipelineConfig(ShaderConfig shaderConfig, Graphics::ComputePipelineDesc desc);

        PipelineConfig(ShaderConfig shaderConfig, Graphics::ComputePipelineDesc desc, std::vector<std::string> macros);

        size_t shaderHash = 0;
        size_t variantHash = 0;

    private:
        void CalculateShaderHash();
        void CalculateVariantHash();

        ShaderConfig shaderConfig;
        std::vector<std::string> macros;

        bool isCompute = false;

        Graphics::GraphicsPipelineDesc graphicsPipelineDesc;
        Graphics::ComputePipelineDesc computePipelineDesc;

    };

}

#endif