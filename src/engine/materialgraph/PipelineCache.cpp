#include "PipelineCache.h"

#include "../pipeline/PipelineManager.h"
#include "../jobsystem/JobSystem.h"
#include "../profiler/CPUProfiler.h"

#include <vector>

namespace Atlas::MaterialGraph {

    bool PipelineCache::asynchronous = true;
    std::mutex PipelineCache::mutex;
    std::unordered_map<Hash, PipelineCache::State> PipelineCache::pipelines;
    JobGroup PipelineCache::jobGroup{ "Material graph pipelines", JobPriority::Medium };

    PipelineCache::State PipelineCache::Request(const PipelineConfig& config, bool blocking) {

        Hash key = 0;
        HashCombine(key, config.shaderHash);
        HashCombine(key, config.variantHash);

        {
            std::scoped_lock lock(mutex);
            auto iter = pipelines.find(key);
            if (iter != pipelines.end() && (iter->second == State::Ready || (!blocking && asynchronous)))
                return iter->second;
        }

        if (blocking || !asynchronous) {
            auto blockingConfig = config;
            return Create(blockingConfig, key);
        }

        {
            std::scoped_lock lock(mutex);
            pipelines[key] = State::Pending;
        }

        // The vertex input of a config points into its vertex array, which refills it for every new config
        const auto& vertexInput = config.graphicsPipelineDesc.vertexInputInfo;
        std::vector<VkVertexInputBindingDescription> bindings(vertexInput.pVertexBindingDescriptions,
            vertexInput.pVertexBindingDescriptions + vertexInput.vertexBindingDescriptionCount);
        std::vector<VkVertexInputAttributeDescription> attributes(vertexInput.pVertexAttributeDescriptions,
            vertexInput.pVertexAttributeDescriptions + vertexInput.vertexAttributeDescriptionCount);

        JobSystem::Execute(jobGroup, [jobConfig = PipelineConfig(config), key, bindings = std::move(bindings),
            attributes = std::move(attributes)](JobData&) mutable {
            auto& jobVertexInput = jobConfig.graphicsPipelineDesc.vertexInputInfo;
            jobVertexInput.pVertexBindingDescriptions = bindings.data();
            jobVertexInput.pVertexAttributeDescriptions = attributes.data();

            Create(jobConfig, key);
            });

        return State::Pending;

    }

    void PipelineCache::Shutdown() {

        JobSystem::Wait(jobGroup);

        std::scoped_lock lock(mutex);
        pipelines.clear();

    }

    PipelineCache::State PipelineCache::Create(PipelineConfig& config, Hash key) {

        auto state = PipelineManager::GetPipeline(config)->isComplete ? State::Ready : State::Failed;

        std::scoped_lock lock(mutex);
        pipelines[key] = state;
        return state;

    }

}
