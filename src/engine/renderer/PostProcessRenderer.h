#pragma once

#include "../System.h"
#include "Renderer.h"
#include "PathTracingRenderer.h"

namespace Atlas {

    namespace Renderer {

        class PostProcessRenderer : public Renderer {

        public:
            PostProcessRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene,
                Graphics::CommandList* commandList, Texture::Texture2D* texture = nullptr);

            void Render(Ref<PathTracerRenderTarget> target, Ref<Scene::Scene> scene,
                Graphics::CommandList* commandList, Texture::Texture2D* texture = nullptr);

        private:
            struct alignas(16) Uniforms {
                float exposure;
                float whitePoint;
                float saturation;
                float contrast;
                float filmGrainStrength;
                int32_t bloomPasses;
                float aberrationStrength;
                float aberrationReversed;
                float vignetteOffset;
                float vignettePower;
                float vignetteStrength;
                vec4 vignetteColor;
            };

            void CopyToTexture(Ref<RenderTarget>& target, Texture::Texture2D* texture,
                Graphics::CommandList* commandList);

            void SetUniforms(const CameraComponent& camera, Ref<Scene::Scene> scene);

            PipelineConfig GetMainPipelineConfig();

            PipelineConfig GetMainPipelineConfig(const Ref<Graphics::FrameBuffer> frameBuffer);

            std::vector<std::string> GetMacros() const;

            PipelineConfig sharpenPipelineConfig;

            Ref<Graphics::MultiBuffer> uniformBuffer;

        };

    }

}