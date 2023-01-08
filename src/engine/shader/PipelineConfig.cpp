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

    PipelineConfig::PipelineConfig(const std::string& shaderFile) : isCompute(true) {

        shaderConfig = {{shaderFile, VK_SHADER_STAGE_COMPUTE_BIT}};

        CalculateShaderHash();
        CalculateVariantHash();

    }

    PipelineConfig::PipelineConfig(const std::string& shaderFile, std::vector<std::string> macros) :
        macros(macros), isCompute(true) {

        shaderConfig = {{shaderFile, VK_SHADER_STAGE_COMPUTE_BIT}};

        CalculateShaderHash();
        CalculateVariantHash();

    }

    bool PipelineConfig::IsValid() const {

        return !shaderConfig.empty();

    }

    void PipelineConfig::AddMacro(const std::string& macro) {

        macros.push_back(macro);
        CalculateVariantHash();

    }

    void PipelineConfig::RemoveMacro(const std::string& macro) {

        auto it = std::find(macros.begin(), macros.end(), macro);

        if (it != macros.end()) {
            macros.erase(it);
            CalculateVariantHash();
        }

    }

    bool PipelineConfig::HasMacro(const std::string& macro) {

        return std::any_of(macros.begin(), macros.end(),
            [macro](const auto& value) { return value == macro; });

    }

    bool PipelineConfig::ManageMacro(const std::string& macro, bool enable) {

        bool hasMacro = HasMacro(macro);
        if (enable && !hasMacro) {
            AddMacro(macro);
        }
        if (!enable && hasMacro) {
            RemoveMacro(macro);
        }
        return hasMacro != enable;

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