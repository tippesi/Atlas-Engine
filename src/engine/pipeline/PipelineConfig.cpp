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

    PipelineConfig::PipelineConfig(const ShaderConfig& shaderConfig, const std::vector<std::string>& macros) :
        shaderConfig(shaderConfig), macros(macros), isCompute(false), isRayTracing(true) {

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

    PipelineConfig PipelineConfig::GetReversedWindingVariant() const {

        auto desc = graphicsPipelineDesc;
        desc.rasterizer.frontFace = desc.rasterizer.frontFace == VK_FRONT_FACE_COUNTER_CLOCKWISE ?
            VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;

        return PipelineConfig(shaderConfig, desc, macros);

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

    bool PipelineConfig::HasMacro(const char* macro) {

        return std::any_of(macros.begin(), macros.end(),
            [macro](const auto& value) { return std::strcmp(value.c_str(), macro) == 0; });

    }

    bool PipelineConfig::ManageMacro(const char* macro, bool enable) {

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
        // Stable, closest hit stage i forms hit group i + 1 with any hit stage i + 1 (RayTracingWorld::materialGraphHitGroups)
        std::stable_sort(shaderConfig.begin(), shaderConfig.end(),
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

        if (!isCompute && !isRayTracing) {
            // To get correct results we would need to hash all the state, but that doesn't
            // really work well in terms of performance.
            HashCombine(variantHash, graphicsPipelineDesc.colorBlendAttachment.blendEnable);
            HashCombine(variantHash, graphicsPipelineDesc.rasterizer.polygonMode);
            HashCombine(variantHash, graphicsPipelineDesc.rasterizer.cullMode);
            HashCombine(variantHash, graphicsPipelineDesc.rasterizer.frontFace);
            HashCombine(variantHash, graphicsPipelineDesc.depthStencilInputInfo.depthTestEnable);
            HashCombine(variantHash, graphicsPipelineDesc.depthStencilInputInfo.depthCompareOp);
            HashCombine(variantHash, graphicsPipelineDesc.depthStencilInputInfo.depthWriteEnable);
            HashCombine(variantHash, graphicsPipelineDesc.assemblyInputInfo.topology);

            for (int32_t i = 0; i < graphicsPipelineDesc.vertexInputInfo.vertexAttributeDescriptionCount; i++) {
                auto& vertexAttributeDesc = graphicsPipelineDesc.vertexInputInfo.pVertexAttributeDescriptions[i];
                HashCombine(variantHash, vertexAttributeDesc.location);
                HashCombine(variantHash, vertexAttributeDesc.format);
            }

            if (!graphicsPipelineDesc.swapChain) {
                // Draws are sorted by this hash, the barrier masks are part of it to keep the established order
                using Graphics::RenderingDesc;
                const auto& formats = graphicsPipelineDesc.attachmentFormats;
                for (int32_t i = 0; i < int32_t(formats.colorAttachmentCount); i++) {
                    if (formats.colorFormats[i] == VK_FORMAT_UNDEFINED) continue;

                    HashCombine(variantHash, i);
                    HashCombine(variantHash, formats.colorFormats[i]);
                    HashCombine(variantHash, RenderingDesc::colorAccessMask);
                    HashCombine(variantHash, RenderingDesc::colorAccessMask);
                    HashCombine(variantHash, RenderingDesc::colorStageMask);
                    HashCombine(variantHash, RenderingDesc::colorStageMask);
                }

                if (formats.depthFormat != VK_FORMAT_UNDEFINED) {
                    HashCombine(variantHash, formats.depthFormat);
                    HashCombine(variantHash, RenderingDesc::depthAccessMask);
                    HashCombine(variantHash, RenderingDesc::depthAccessMask);
                    HashCombine(variantHash, RenderingDesc::depthStageMask);
                    HashCombine(variantHash, RenderingDesc::depthStageMask);
                }
            }
            else {
                HashCombine(variantHash, graphicsPipelineDesc.swapChain->renderPass);
            }
        }

    }


}
