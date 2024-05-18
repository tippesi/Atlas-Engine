#pragma once

#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        class GBufferRenderer : public Renderer {

        public:
            GBufferRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Downscale(const Ref<RenderTarget>& target, Graphics::CommandList* commandList);

            void DownscaleDepthOnly(const Ref<RenderTarget>& target, Graphics::CommandList* commandList);

            void FillNormalTexture(const Ref<RenderTarget>& target, Graphics::CommandList* commandList);

            void GenerateReactiveMask(const Ref<RenderTarget>& target, Graphics::CommandList* commandList);

        private:
            void Downscale(RenderTargetData* rt, RenderTargetData* downsampledRt, Graphics::CommandList* commandList);

            PipelineConfig downscalePipelineConfig;
            PipelineConfig downscaleDepthOnlyPipelineConfig;
            PipelineConfig patchNormalPipelineConfig;
            PipelineConfig generateReactiveMaskPipelineConfig;

        };

    }

}