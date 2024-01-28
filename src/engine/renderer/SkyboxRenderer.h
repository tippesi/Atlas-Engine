#pragma once

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

            void Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList);

        private:
            PipelineConfig pipelineConfig;

        };

    }

}