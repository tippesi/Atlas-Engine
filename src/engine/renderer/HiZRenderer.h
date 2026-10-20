#pragma once

#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        class HiZRenderer : public Renderer {

        public:
            HiZRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Generate(const RenderTargetData* data, float thickness, Graphics::CommandList* commandList);

        private:
            struct alignas(16) PushConstants {
                ivec2 inputSize;
                int32_t fromDepth;
                int32_t levelCount;
                float thickness;
            };

            PipelineConfig pipelineConfig;

        };

    }

}
