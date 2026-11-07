#pragma once

#include "../System.h"
#include "UpscalingRenderer.h"

namespace Atlas {

    namespace Renderer {

        class TemporalAARenderer : public UpscalingRenderer {

        public:
            TemporalAARenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Release() override { historyInvalid = true; }

            vec2 GetJitter(const Ref<RenderTarget>& target, uint32_t frameIndex) const override;

            void Render(const Ref<RenderTarget>& target, const Ref<Scene::Scene>& scene, Graphics::CommandList* commandList) override;

        private:
            struct alignas(16) PushConstants {
                vec2 resolution;
                vec2 invResolution;
                vec2 jitter;
                int32_t resetHistory;
            };

            PipelineConfig pipelineConfig;

            std::vector<vec2> haltonSequence;

            bool historyInvalid = false;

        };

    }

}
