#pragma once

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        class TemporalAARenderer : public Renderer {

        public:
            TemporalAARenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList);

        private:
            struct alignas(16) PushConstants {
                vec2 resolution;
                vec2 invResolution;
                vec2 jitter;
                int32_t resetHistory;
            };

            PipelineConfig pipelineConfig;

        };

    }

}
