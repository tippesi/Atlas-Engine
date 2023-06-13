#ifndef AE_SKYBOXRENDERER_H
#define AE_SKYBOXRENDERER_H

#include "../System.h"
#include "Renderer.h"
#include "buffer/VertexArray.h"

#include <unordered_map>

namespace Atlas {

    namespace Renderer {

        class SkyboxRenderer : public Renderer {

        public:
            SkyboxRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene,
                Graphics::CommandList* commandList);

        private:
            PipelineConfig pipelineConfig;

        };

    }

}

#endif