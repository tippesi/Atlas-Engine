#include "PipelineManager.h"

#include "../common/Hash.h"
#include "../loader/ShaderLoader.h"
#include "../graphics/GraphicsDevice.h"

namespace Atlas {

#ifdef AE_BUILDTYPE_RELEASE
    std::atomic_bool PipelineManager::hotReload = false;
#else
    std::atomic_bool PipelineManager::hotReload = true;
#endif
    std::shared_mutex PipelineManager::shaderToVariantsMutex;
    std::unordered_map<size_t, Ref<PipelineManager::PipelineVariants>> PipelineManager::shaderToVariantsMap;
    Ref<Graphics::DescriptorSetLayout> PipelineManager::globalDescriptorSetLayoutOverrides[DESCRIPTOR_SET_COUNT];
    JobGroup PipelineManager::hotReloadGroup{ "Shader hot reload" };

    void PipelineManager::Init() {

        for (uint32_t i = 0; i < DESCRIPTOR_SET_COUNT; i++) {
            globalDescriptorSetLayoutOverrides[i] = nullptr;
        }

    }

    void PipelineManager::Shutdown() {

        Clear();

    }

    void PipelineManager::Clear() {

        std::unique_lock lock(shaderToVariantsMutex);

        for (uint32_t i = 0; i < DESCRIPTOR_SET_COUNT; i++) {
            globalDescriptorSetLayoutOverrides[i] = nullptr;
        }

        shaderToVariantsMap.clear();

    }

    void PipelineManager::Update() {

        if (hotReload && hotReloadGroup.HasFinished()) {

            JobSystem::Execute(hotReloadGroup, [&](JobData&) {
                std::shared_lock lock(shaderToVariantsMutex);

                std::unordered_map<std::string, std::filesystem::file_time_type> modifiedMap;

                for (auto& [_, variants] : shaderToVariantsMap) {
                    std::lock_guard innerLock(variants->variantsMutex);
                    if (!variants->isComplete) continue;

                    for (auto& stageFile : variants->shader->shaderStageFiles) {
                        for (auto& includePath : stageFile.includes) {
                            if (modifiedMap.find(includePath) != modifiedMap.end())
                                continue;

                            std::error_code errorCode;
                            auto lastModified = std::filesystem::last_write_time(includePath, errorCode);
                            if (!errorCode)
                                modifiedMap[includePath] = lastModified;
                        }
                    }
                }

                // Do check for hot reload only once per frame
                for (auto& [hash, variants] : shaderToVariantsMap) {
                    std::lock_guard innerLock(variants->variantsMutex);
                    if (!variants->isComplete) continue;

                    if (hotReload && variants->shader->Reload(modifiedMap)) {
                        // Clear variants on hot reload
                        variants->pipelines.clear();
                    }
                }
            });
        }

    }

    void PipelineManager::EnableHotReload() {

        hotReload = true;

    }

    void PipelineManager::DisableHotReload() {

        hotReload = false;

    }

    void PipelineManager::AddPipeline(PipelineConfig& config) {

        GetOrCreatePipeline(config);

    }

    Ref<Graphics::Pipeline> PipelineManager::GetPipeline(PipelineConfig& config) {

        return GetOrCreatePipeline(config);

    }

    void PipelineManager::OverrideDescriptorSetLayout(Ref<Graphics::DescriptorSetLayout> layout, uint32_t set) {

        std::unique_lock lock(shaderToVariantsMutex);

        if (set < DESCRIPTOR_SET_COUNT) {

            globalDescriptorSetLayoutOverrides[set] = layout;

            for (auto& [_, variants] : shaderToVariantsMap) {

                std::unordered_map<size_t, Ref<Graphics::Pipeline>> validPipelines;

                std::lock_guard innerLock(variants->variantsMutex);

                for (auto& [pipelineHash, pipeline] : variants->pipelines) {

                    // Note: We need to only recreate pipelines where the override layout
                    // is compatible with the shader. Other pipelines should remain valid
                    // Also: We might override a layout per shader several times, but that doesn't hurt
                    if (!pipeline->shader->TryOverrideDescriptorSetLayout(layout, set)) {
                        validPipelines[pipelineHash] = pipeline;
                    }

                }

                // Overwrite old piplines
                variants->pipelines = validPipelines;

            }

        }

    }

    Ref<Graphics::Pipeline> PipelineManager::GetOrCreatePipeline(PipelineConfig& config) {

        auto graphicsDevice = Graphics::GraphicsDevice::DefaultDevice;

        // Keep the cache entry and global layout overrides stable while using them.
        std::shared_lock mapLock(shaderToVariantsMutex);
        auto variantsIt = shaderToVariantsMap.find(config.shaderHash);
        while (variantsIt == shaderToVariantsMap.end()) {
            mapLock.unlock();
            {
                std::unique_lock insertLock(shaderToVariantsMutex);
                // Another caller may have inserted this shader while we changed locks.
                if (!shaderToVariantsMap.contains(config.shaderHash))
                    shaderToVariantsMap.emplace(config.shaderHash, std::make_shared<PipelineVariants>());
            }
            mapLock.lock();
            // Clear() may have run between releasing the exclusive lock and reacquiring this lock.
            variantsIt = shaderToVariantsMap.find(config.shaderHash);
        }
        const auto& variants = variantsIt->second;

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
            if (!variants->pipelines.contains(config.variantHash)) {
                auto shaderVariant = variants->shader->GetVariant(config.macros);

                for (uint32_t i = 0; i < DESCRIPTOR_SET_COUNT; i++) {
                    auto& layout = globalDescriptorSetLayoutOverrides[i];
                    if (layout == nullptr) continue;

                    // Only override layout if it is compatible
                    if (shaderVariant->sets[i].bindingCount > 0) {
                        shaderVariant->TryOverrideDescriptorSetLayout(layout, i);
                    }
                }

                if (config.isCompute) {
                    auto pipelineDesc = Graphics::ComputePipelineDesc{
                        .shader = shaderVariant
                    };
                    pipeline = graphicsDevice->CreatePipeline(pipelineDesc);
                }
                else {
                    config.graphicsPipelineDesc.shader = shaderVariant;
                    pipeline = graphicsDevice->CreatePipeline(config.graphicsPipelineDesc);
                }
                variants->pipelines[config.variantHash] = pipeline;
            }
            else {
                pipeline = variants->pipelines[config.variantHash];
            }

            return pipeline;
        }

    }

}
