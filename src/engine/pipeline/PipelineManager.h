#pragma once

#include "PipelineConfig.h"

#include "../graphics/Pipeline.h"

#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <utility>

namespace Atlas {

    class PipelineManager {

    public:
        static void Init();

        static void Shutdown();

        static void Update();

        static void EnableHotReload();

        static void DisableHotReload();

        static Ref<Graphics::Pipeline> GetPipeline(PipelineConfig& config);

        static void AddPipeline(PipelineConfig& config);

    private:
        struct PipelineVariants {
            Ref<Graphics::Shader> shader;

            std::mutex variantsMutex;
            std::unordered_map<size_t, Ref<Graphics::Pipeline>> variants;

            bool isComplete = false;
        };

        static Ref<Graphics::Pipeline> GetOrCreatePipeline(PipelineConfig &config);

        static bool hotReload;
        static std::mutex shaderToVariantsMutex;
        static std::unordered_map<size_t, Ref<PipelineVariants>> shaderToVariantsMap;

    };

}