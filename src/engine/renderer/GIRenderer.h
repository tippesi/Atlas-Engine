#pragma once

#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        class GIRenderer : public Renderer {

            GIRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene);

        private:
            Graphics::GraphicsDevice* device;

        };

    }

}