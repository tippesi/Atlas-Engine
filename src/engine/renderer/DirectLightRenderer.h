#pragma once

#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        class DirectLightRenderer : public Renderer {

        public:
            DirectLightRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, 
                Graphics::CommandList* commandList);

        private:
            struct alignas(16) CullingPushConstants {
                int32_t lightCount;
            };

            struct alignas(16) PushConstants {
                int32_t lightCount;
                int32_t lightBucketCount;
                int32_t padding1;
                int32_t padding2;
                int32_t mapIndices[16];
            };

            PipelineConfig pipelineConfig;

            Buffer::Buffer lightCullingBuffer;
            Buffer::UniformBuffer cloudShadowUniformBuffer;
            Ref<Graphics::Sampler> shadowSampler;

        };

    }

}