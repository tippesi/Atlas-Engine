#pragma once

#include "../../System.h"
#include "../../buffer/Buffer.h"
#include "../../pipeline/PipelineConfig.h"
#include "../../graphics/CommandList.h"

namespace Atlas {

    namespace Renderer {

        namespace Helper {

            // With software ray tracing the traces only store the shadow rays of their light samples, a pass of its own
            // traces them and writes the visibility into the light samples, a resolve reads them
            class DeferredShadowRays {

            public:
                void Init();

                // Sizes the buffers for the samples of a trace and resets the ray count
                void Prepare(Graphics::CommandList* commandList, size_t sampleCount, size_t lightCount);

                void BindForTrace(Graphics::CommandList* commandList);

                void BindForResolve(Graphics::CommandList* commandList);

                void Trace(Graphics::CommandList* commandList);

            private:
                PipelineConfig pipelineConfig;

                Buffer::Buffer sampleBuffer;
                Buffer::Buffer lightBuffer;
                Buffer::Buffer rayBuffer;
                Buffer::Buffer rayCountBuffer;

                size_t lightCount = 0;

            };

        }

    }

}
