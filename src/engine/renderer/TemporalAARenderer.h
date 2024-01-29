#pragma once

#include "../System.h"
#include "Renderer.h"
#include "PathTracingRenderer.h"

namespace Atlas {

    namespace Renderer {

        class TemporalAARenderer : public Renderer {

        public:
            TemporalAARenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList);

            void Render(Ref<PathTracerRenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList);

        private:
            struct PushConstants {
                vec2 resolution;
                vec2 invResolution;
                vec2 jitter;
            };

            void Render(const Texture::Texture2D* outputTexture, const Texture::Texture2D* currentTexture, const Texture::Texture2D* historyTexture,
                const Texture::Texture2D* velocityTexture, const Texture::Texture2D* historyVelocityTexture, const Texture::Texture2D* depthTexture,
                const Texture::Texture2D* stencilTexture, PushConstants& constants, Graphics::CommandList* commandList);

            PipelineConfig pipelineConfig;

        };

    }

}