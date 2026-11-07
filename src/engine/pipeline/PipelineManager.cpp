#include "PipelineManager.h"

#include "../Clock.h"
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
    std::mutex PipelineManager::globalLayoutOverridesMutex;
    Ref<Graphics::DescriptorSetLayout> PipelineManager::globalDescriptorSetLayoutOverrides[DESCRIPTOR_SET_COUNT];
    JobGroup PipelineManager::hotReloadGroup{ "Shader hot reload" };
    float PipelineManager::lastHotReloadCheck = 0.0f;

    void PipelineManager::Init() {

        for (uint32_t i = 0; i < DESCRIPTOR_SET_COUNT; i++) {
            globalDescriptorSetLayoutOverrides[i] = nullptr;
        }

    }

    void PipelineManager::Shutdown() {

        Clear();

    }

    void PipelineManager::Clear() {

        {
            std::scoped_lock lock(globalLayoutOverridesMutex);
            for (uint32_t i = 0; i < DESCRIPTOR_SET_COUNT; i++) {
                globalDescriptorSetLayoutOverrides[i] = nullptr;
            }
        }

        std::unique_lock lock(shaderToVariantsMutex);
        shaderToVariantsMap.clear();

    }

    void PipelineManager::Update() {

        // Going through all files every frame takes milliseconds, a change only has to show up soon
        auto time = Clock::Get();
        if (hotReload && hotReloadGroup.HasFinished() && time - lastHotReloadCheck >= hotReloadInterval) {
            lastHotReloadCheck = time;

            JobSystem::Execute(hotReloadGroup, [&](JobData&) {
                auto allVariants = GetAllVariants();

                std::unordered_map<std::string, std::filesystem::file_time_type> modifiedMap;

                for (auto& variants : allVariants) {
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

                for (auto& variants : allVariants) {
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

        if (set < DESCRIPTOR_SET_COUNT) {

            {
                std::scoped_lock lock(globalLayoutOverridesMutex);
                globalDescriptorSetLayoutOverrides[set] = layout;
            }

            // A pipeline being created right now either has the new layout already or is checked here afterwards
            for (auto& variants : GetAllVariants()) {

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

        auto variants = GetOrCreateVariants(config.shaderHash);

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

                {
                    std::scoped_lock overridesLock(globalLayoutOverridesMutex);
                    for (uint32_t i = 0; i < DESCRIPTOR_SET_COUNT; i++) {
                        auto& layout = globalDescriptorSetLayoutOverrides[i];
                        if (layout == nullptr) continue;

                        // Only override layout if it is compatible
                        if (shaderVariant->sets[i].bindingCount > 0) {
                            shaderVariant->TryOverrideDescriptorSetLayout(layout, i);
                        }
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

    Ref<PipelineManager::PipelineVariants> PipelineManager::GetOrCreateVariants(size_t shaderHash) {

        {
            std::shared_lock lock(shaderToVariantsMutex);
            auto iter = shaderToVariantsMap.find(shaderHash);
            if (iter != shaderToVariantsMap.end())
                return iter->second;
        }

        std::unique_lock lock(shaderToVariantsMutex);
        auto& variants = shaderToVariantsMap[shaderHash];
        if (!variants)
            variants = std::make_shared<PipelineVariants>();
        return variants;

    }

    std::vector<Ref<PipelineManager::PipelineVariants>> PipelineManager::GetAllVariants() {

        std::shared_lock lock(shaderToVariantsMutex);

        std::vector<Ref<PipelineVariants>> allVariants;
        allVariants.reserve(shaderToVariantsMap.size());
        for (const auto& [_, variants] : shaderToVariantsMap)
            allVariants.push_back(variants);

        return allVariants;

    }

}
