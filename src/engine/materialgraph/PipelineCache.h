#pragma once

#include "../pipeline/PipelineConfig.h"
#include "../jobsystem/JobGroup.h"

#include <mutex>
#include <unordered_map>

namespace Atlas::MaterialGraph {

    /**
     * Creates the pipelines of graphs in the background. Compiling generated shaders takes long enough to be visible
     * as a hitch, meanwhile a mesh draws with the regular pipeline of its material. It keeps doing that if they fail.
     */
    class PipelineCache {

    public:
        enum class State {
            Pending = 0,
            Ready,
            Failed
        };

        // The state of the pipeline, requests it if that didn't happen yet. The config needs its frame buffer
        // Blocking requests try failed pipelines again, a change of a shader could have fixed them
        static State Request(const PipelineConfig& config, bool blocking);

        // The state of a single shader stage, compiled in the background without macros
        static State RequestStage(const std::string& file, VkShaderStageFlagBits stage);

        static void Shutdown();

        // Tools rendering single frames disable this, pictures should never show the fallback
        static bool asynchronous;

    private:
        static State Create(PipelineConfig& config, Hash key);

        static std::mutex mutex;
        static std::unordered_map<Hash, State> pipelines;
        static JobGroup jobGroup;

    };

}
