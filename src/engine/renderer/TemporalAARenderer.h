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

            void Render(Viewport* viewport, RenderTarget* target, Camera* camera,
                Scene::Scene* scene, Graphics::CommandList* commandList);

            void Render(Viewport* viewport, PathTracerRenderTarget* target, Camera* camera,
                Scene::Scene* scene, Graphics::CommandList* commandList);

        private:
            struct PushConstants {
                vec2 resolution;
                vec2 invResolution;
                vec2 jitter;
            };

            void Render(Texture::Texture2D* outputTexture, Texture::Texture2D* currentTexture, Texture::Texture2D* historyTexture,
                Texture::Texture2D* velocityTexture, Texture::Texture2D* historyVelocityTexture, Texture::Texture2D* depthTexture,
                Texture::Texture2D* stencilTexture, PushConstants& constants, Graphics::CommandList* commandList);

            PipelineConfig pipelineConfig;

        };

    }

}