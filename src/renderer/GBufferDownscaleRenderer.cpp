#include "GBufferDownscaleRenderer.h"

namespace Atlas {

    namespace Renderer {

        GBufferDownscaleRenderer::GBufferDownscaleRenderer() {

            shader.AddStage(AE_COMPUTE_STAGE, "downsampleGBuffer2x.csh");

        }

        void GBufferDownscaleRenderer::Render(Viewport *viewport, RenderTarget *target, 
            Camera *camera, Scene::Scene *scene) {

            shader.Bind();

            

        }

    }

}