#include "PipelineConfig.h"

#include "../common/Hash.h"

#include <algorithm>

namespace Atlas {

    PipelineConfig::PipelineConfig(ShaderConfig shaderConfig, Graphics::GraphicsPipelineDesc desc) :
        shaderConfig(shaderConfig), graphicsPipelineDesc(desc), isCompute(false) {

        CalculateShaderHash();
        CalculateVariantHash();

    }

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

    bool PipelineConfig::IsValid() const {

        return !shaderConfig.empty();

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

        // For now, we can ignore the vertex input in the variant hash calculation
        // since for our use case we always have the same input for the same shader
        // except when a macro disables an input.
        variantHash = 0;
        std::sort(macros.begin(), macros.end());

        for (auto& macro : macros) {
            HashCombine(variantHash, macro);
        }

        if (!isCompute) {
            HashCombine(variantHash, graphicsPipelineDesc.rasterizer.cullMode);
        }

    }


}