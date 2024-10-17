#pragma once

#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        class SSSRenderer : public Renderer {

        public:
            SSSRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList);

        private:
            struct alignas(16) PushConstants {
                vec4 lightDirection;
                int sampleCount;
                float maxLength;
                float minLengthWorldSpace;
                float thickness;
            };

            Filter blurFilter;

            PipelineConfig pipelineConfig;

        };


    }

}