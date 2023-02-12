#ifndef ATLASENGINE_VULKANTESTRENDERER_H
#define ATLASENGINE_VULKANTESTRENDERER_H

#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        class ExampleRenderer : public Renderer {

        public:
            ExampleRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Camera* camera);

        private:
            Ref<Graphics::Shader> shader = nullptr;
            Ref<Graphics::Shader> meshShader = nullptr;
            Ref<Graphics::Shader> computeShader = nullptr;
            Ref<Graphics::Pipeline> pipeline = nullptr;
            Ref<Graphics::Pipeline> meshPipeline = nullptr;
            Ref<Graphics::Pipeline> computePipeline = nullptr;
            Ref<Graphics::Buffer> buffer = nullptr;
            Ref<Graphics::FrameBuffer> mainFrameBuffer = nullptr;

            Ref<Graphics::MultiBuffer> uniformBuffer = nullptr;
            Ref<Graphics::RenderPass> mainRenderPass = nullptr;
            Ref<Graphics::Sampler> mainRenderPassSampler = nullptr;
            Ref<Graphics::Image> dstImage = nullptr;

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
