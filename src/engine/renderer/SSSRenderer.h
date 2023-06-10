#ifndef AE_SSSRENDERER_H
#define AE_SSSRENDERER_H

#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        class SSSRenderer : public Renderer {

        public:
            SSSRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Viewport* viewport, RenderTarget* target, Camera* camera,
                Scene::Scene* scene, Graphics::CommandList* commandList);

        private:
            struct alignas(16) PushConstants {
                vec4 lightDirection;
                int sampleCount;
                float maxLength;
                float thickness;
            };

            Filter blurFilter;

            PipelineConfig pipelineConfig;

        };


    }

}


#endif