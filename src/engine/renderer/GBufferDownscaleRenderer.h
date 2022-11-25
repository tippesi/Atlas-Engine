#ifndef AE_GBUFFERDOWNSCALERENDERER_H
#define AE_GBUFFERDOWNSCALERENDERER_H

#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        class GBufferDownscaleRenderer : public Renderer {

        public:
            GBufferDownscaleRenderer();

            void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) final;

            void Downscale(RenderTarget* target);

            void DownscaleDepthOnly(RenderTarget* target);

        private:
            void Downscale(DownsampledRenderTarget* rt, DownsampledRenderTarget* downsampledRt);

            Shader::Shader downscale;
            Shader::Shader downscaleDepthOnly;

        };

    }

}

#endif
