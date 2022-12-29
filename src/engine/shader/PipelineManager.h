#ifndef AE_PIPELINEMANAGER_H
#define AE_PIPELINEMANAGER_H

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

        static std::mutex shaderToVariantsMutex;
        static std::unordered_map<size_t, Ref<PipelineVariants>> shaderToVariantsMap;

    };

}

#endif
