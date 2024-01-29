#pragma once

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        class IndirectLightRenderer : public Renderer {

        public:
            IndirectLightRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList);

        private:
            struct Uniforms {
                int aoEnabled;
                int aoDownsampled2x;
                int reflectionEnabled;
                float aoStrength;
                int specularProbeMipLevels;
            };

            PipelineConfig pipelineConfig;

            Buffer::UniformBuffer uniformBuffer;

        };

    }

}