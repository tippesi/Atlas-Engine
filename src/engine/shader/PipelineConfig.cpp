#include "PipelineConfig.h"

#include "../common/Hash.h"

namespace Atlas {

    PipelineConfig::PipelineConfig(ShaderConfig shaderConfig, Graphics::GraphicsPipelineDesc desc,
        std::vector<std::string> macros) : shaderConfig(shaderConfig), macros(macros),
                                           graphicsPipelineDesc(desc), isCompute(false) {

        CalculateShaderHash();
        CalculateVariantHash();

    }

    PipelineConfig::PipelineConfig(ShaderConfig shaderConfig, Graphics::ComputePipelineDesc desc) :
        shaderConfig(shaderConfig), computePipelineDesc(desc), isCompute(true) {

        CalculateShaderHash();
        CalculateVariantHash();

    }

    PipelineConfig::PipelineConfig(ShaderConfig shaderConfig, Graphics::ComputePipelineDesc desc,
        std::vector<std::string> macros) : shaderConfig(shaderConfig), macros(macros),
                                           computePipelineDesc(desc), isCompute(true) {

        CalculateShaderHash();
        CalculateVariantHash();

    }

    void PipelineConfig::CalculateShaderHash() {

        shaderHash = 0;
        std::sort(shaderConfig.begin(), shaderConfig.end(),
            [](auto& stage0, auto& stage1) {
                return stage0.second < stage1.second;
            });

        for (auto& [name, _] : shaderConfig) {
            HashCombine(shaderHash, name);
        }

    }

    void PipelineConfig::CalculateVariantHash() {

        variantHash = 0;
        std::sort(macros.begin(), macros.end());

        for (auto& macro : macros) {
            HashCombine(variantHash, macro);
        }

    }


}