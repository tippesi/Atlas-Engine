#include "PipelineManager.h"

#include "../common/Hash.h"
#include "../loader/ShaderLoader.h"
#include "../graphics/GraphicsDevice.h"

namespace Atlas {

#ifdef AE_BUILDTYPE_RELEASE
    bool PipelineManager::hotReload = false;
#else
    bool PipelineManager::hotReload = true;
#endif
    std::mutex PipelineManager::shaderToVariantsMutex;
    std::unordered_map<size_t, Ref<PipelineManager::PipelineVariants>> PipelineManager::shaderToVariantsMap;

    void PipelineManager::Init() {



    }

    void PipelineManager::Shutdown() {

        shaderToVariantsMap.clear();

    }

    void PipelineManager::Update() {

        std::lock_guard lock(shaderToVariantsMutex);

        std::unordered_map<std::string, std::filesystem::file_time_type> lastModifiedMap;
        if (hotReload) {
            for (auto& [_, variants] : shaderToVariantsMap) {
                for (auto& stageFile : variants->shader->shaderStageFiles) {
                    for (auto& includePath : stageFile.includes) {
                        if (lastModifiedMap.find(includePath) != lastModifiedMap.end())
                            continue;

                        std::error_code errorCode;
                        auto lastModified = std::filesystem::last_write_time(includePath, errorCode);
                        if (!errorCode)
                            lastModifiedMap[includePath] = lastModified;
                    }
                }
            }
        }

        // Do check for hot reload only once per frame
        for (auto& [hash, variants] : shaderToVariantsMap) {
            if (hotReload && variants->shader->Reload(lastModifiedMap)) {
                // Clear variants on hot reload
                variants->variants.clear();
            }
        }

    }

    void PipelineManager::EnableHotReload() {

        hotReload = true;

    }

    void PipelineManager::DisableHotReload() {

        hotReload = false;

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