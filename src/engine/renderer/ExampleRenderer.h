#pragma once

#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        class ExampleRenderer {

        public:
            ExampleRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(const CameraComponent& camera);

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

            ResourceHandle<Mesh::Mesh> mesh;

            struct alignas(16) PushConstants {
                mat4 vMatrix;
                mat4 pMatrix;
            };

            struct Uniforms {
                mat4 vMatrix;
                mat4 pMatrix;
            };

            struct alignas(16) ComputeConstants {
                float randomSeed;
                float time;
            };

        };

    }

}
