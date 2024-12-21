#pragma once

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        class IndirectLightRenderer : public Renderer {

        public:
            IndirectLightRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void RenderIndirectDiffuse(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList);

            void RenderIndirectSpecular(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList);

        private:
            struct Uniforms {
                int reflectionDownsampled2x;
                int giDownsampled2x;
                float aoStrength;
                int specularProbeMipLevels;
            };

            PipelineConfig diffusePipelineConfig;
            PipelineConfig specularPipelineConfig;

            Buffer::UniformBuffer uniformBuffer;

        };

    }

}