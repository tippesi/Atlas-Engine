#ifndef AE_GBUFFERDOWNSCALERENDERER_H
#define AE_GBUFFERDOWNSCALERENDERER_H

#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        class GBufferDownscaleRenderer : public Renderer {

        public:
            GBufferDownscaleRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Downscale(RenderTarget* target, Graphics::CommandList* commandList);

            void DownscaleDepthOnly(RenderTarget* target, Graphics::CommandList* commandList);

        private:
            void Downscale(RenderTargetData* rt, RenderTargetData* downsampledRt, Graphics::CommandList* commandList);

            PipelineConfig downscalePipelineConfig;
            PipelineConfig downscaleDepthOnlyPipelineConfig;

            //OldShader::OldShader downscale;
            //OldShader::OldShader downscaleDepthOnly;

        };

    }

}

#endif
