#pragma once

#include "Renderer.h"
#include "helper/LightData.h"

namespace Atlas {

    namespace Renderer {

        class DirectLightRenderer : public Renderer {

        public:
            DirectLightRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, 
                Helper::LightData& lightData, Graphics::CommandList* commandList);

        private:
            struct alignas(16) Uniforms {
                int32_t mapIndices[16];
                int32_t lightCount;
            };

            PipelineConfig pipelineConfig;

            Buffer::UniformBuffer uniformBuffer;
            Buffer::UniformBuffer cloudShadowUniformBuffer;
            Ref<Graphics::Sampler> shadowSampler;

        };

    }

}