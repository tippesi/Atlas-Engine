#ifndef ATLASENGINE_VULKANTESTRENDERER_H
#define ATLASENGINE_VULKANTESTRENDERER_H

#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        class ExampleRenderer {

        public:
            ExampleRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Camera* camera);

        private:
            Graphics::GraphicsDevice* device = nullptr;

            Ref<Graphics::Pipeline> pipeline = nullptr;
            Ref<Graphics::Pipeline> meshPipeline = nullptr;
            Ref<Graphics::Pipeline> computePipeline = nullptr;
            Ref<Graphics::Buffer> buffer = nullptr;
            Ref<Graphics::FrameBuffer> mainFrameBuffer = nullptr;

            Ref<Graphics::MultiBuffer> uniformBuffer = nullptr;
            Ref<Graphics::RenderPass> mainRenderPass = nullptr;
            Ref<Graphics::Sampler> linearSampler = nullptr;
            Ref<Graphics::Image> destinationImage = nullptr;

            Ref<Graphics::QueryPool> queryPool = nullptr;

            Ref<Mesh::Mesh> mesh = nullptr;

            struct PushConstants {
                mat4 vMatrix;
                mat4 pMatrix;
            };

            struct Uniforms {
                mat4 vMatrix;
                mat4 pMatrix;
            };

            struct ComputeConstants {
                float randomSeed;
                float time;
            };

        };

    }

}

#endif
