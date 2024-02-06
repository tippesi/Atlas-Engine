#include "PipelineConfig.h"

#include "../common/Hash.h"
#include "../graphics/SwapChain.h"

#include <algorithm>

namespace Atlas {

    PipelineConfig::PipelineConfig(const ShaderConfig& shaderConfig, const Graphics::GraphicsPipelineDesc& desc) :
        shaderConfig(shaderConfig), graphicsPipelineDesc(desc), isCompute(false) {

        CalculateShaderHash();
        CalculateVariantHash();

    }

    PipelineConfig::PipelineConfig(const ShaderConfig& shaderConfig, const Graphics::GraphicsPipelineDesc& desc,
        const std::vector<std::string>& macros) : shaderConfig(shaderConfig), macros(macros),
        graphicsPipelineDesc(desc), isCompute(false) {

        CalculateShaderHash();
        CalculateVariantHash();

    }

    PipelineConfig::PipelineConfig(const std::string& shaderFile) : isCompute(true) {

        shaderConfig = {{shaderFile, VK_SHADER_STAGE_COMPUTE_BIT}};

        CalculateShaderHash();
        CalculateVariantHash();

    }

    PipelineConfig::PipelineConfig(const std::string& shaderFile, const std::vector<std::string>& macros) :
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
            [](const auto& stage0, const auto& stage1) {
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
            auto renderPass = graphicsPipelineDesc.frameBuffer ?
                              graphicsPipelineDesc.frameBuffer->renderPass->renderPass
                              : graphicsPipelineDesc.swapChain->renderPass;

            // To get correct results we would need to hash all the state, but that doesn't
            // really work well in terms of performance
            HashCombine(variantHash, renderPass);
            HashCombine(variantHash, graphicsPipelineDesc.colorBlendAttachment.blendEnable);
            HashCombine(variantHash, graphicsPipelineDesc.rasterizer.polygonMode);
            HashCombine(variantHash, graphicsPipelineDesc.rasterizer.cullMode);
            HashCombine(variantHash, graphicsPipelineDesc.depthStencilInputInfo.depthTestEnable);
            HashCombine(variantHash, graphicsPipelineDesc.depthStencilInputInfo.depthCompareOp);
            HashCombine(variantHash, graphicsPipelineDesc.depthStencilInputInfo.depthWriteEnable);
            HashCombine(variantHash, graphicsPipelineDesc.assemblyInputInfo.topology);

            for (int32_t i = 0; i < graphicsPipelineDesc.vertexInputInfo.vertexAttributeDescriptionCount; i++) {
                auto& vertexAttributeDesc = graphicsPipelineDesc.vertexInputInfo.pVertexAttributeDescriptions[i];
                HashCombine(variantHash, vertexAttributeDesc.format);
            }
        }

    }


}