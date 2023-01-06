#include "PipelineManager.h"

#include "../common/Hash.h"
#include "../loader/ShaderLoader.h"
#include "../graphics/GraphicsDevice.h"

namespace Atlas {

    bool PipelineManager::hotReload = false;
    std::mutex PipelineManager::shaderToVariantsMutex;
    std::unordered_map<size_t, Ref<PipelineManager::PipelineVariants>> PipelineManager::shaderToVariantsMap;

    void PipelineManager::Init() {



    }

    void PipelineManager::Shutdown() {

        shaderToVariantsMap.clear();

    }

    void PipelineManager::AddPipeline(PipelineConfig &config) {

        GetOrCreatePipeline(config);

    }

    Ref<Graphics::Pipeline> PipelineManager::GetPipeline(PipelineConfig &config) {

        return GetOrCreatePipeline(config);

    }

    Ref<Graphics::Pipeline> PipelineManager::GetOrCreatePipeline(PipelineConfig &config) {

        auto graphicsDevice = Graphics::GraphicsDevice::DefaultDevice;

        PipelineVariants* variants;
        {
            std::lock_guard lock(shaderToVariantsMutex);

            if (!shaderToVariantsMap.contains(config.shaderHash)) {
                auto pipelineVariants = std::make_shared<PipelineVariants>();
                shaderToVariantsMap[config.shaderHash] = pipelineVariants;
                variants = pipelineVariants.get();
            }
            else {
                variants = shaderToVariantsMap[config.shaderHash].get();
            }
        }

        {
            std::lock_guard lock(variants->variantsMutex);

            if (!variants->isComplete) {
                std::vector<Graphics::ShaderStageFile> stages;
                for (auto& [name, stageDef] : config.shaderConfig) {
                    stages.push_back(Loader::ShaderLoader::LoadFile(name, stageDef));
                }
                auto shaderDesc = Graphics::ShaderDesc{
                    .stages = stages
                };
                variants->shader = graphicsDevice->CreateShader(shaderDesc);
                variants->isComplete = true;
            }

            if (hotReload && variants->shader->Reload()) {
                // Clear variants on hot reload
                variants->variants.clear();
            }

            Ref<Graphics::Pipeline> pipeline;
            if (!variants->variants.contains(config.variantHash)) {
                if (config.isCompute) {
                    auto pipelineDesc = Graphics::ComputePipelineDesc {
                        .shader = variants->shader->GetVariant(config.macros)
                    };
                    pipeline = graphicsDevice->CreatePipeline(pipelineDesc);
                }
                else {
                    config.graphicsPipelineDesc.shader = variants->shader->GetVariant(config.macros);
                    pipeline = graphicsDevice->CreatePipeline(config.graphicsPipelineDesc);
                }
                variants->variants[config.variantHash] = pipeline;
            }
            else {
                pipeline = variants->variants[config.variantHash];
            }

            return pipeline;
        }

    }

}