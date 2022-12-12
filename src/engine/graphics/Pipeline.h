#ifndef AE_GRAPHICSPIPELINE_H
#define AE_GRAPHICSPIPELINE_H

#include "Common.h"
#include "MemoryManager.h"

namespace Atlas {

    namespace Graphics {

        struct PipelineDesc {

        };

        class Pipeline {
        public:
            Pipeline(MemoryManager* memManager, PipelineDesc desc);

            VkPipeline pipeline;
            VkPipelineLayout pipelineLayout;

            bool isComplete = false;

        private:


        };

    }

}

#endif