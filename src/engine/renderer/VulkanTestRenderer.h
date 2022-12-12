#ifndef ATLASENGINE_VULKANTESTRENDERER_H
#define ATLASENGINE_VULKANTESTRENDERER_H

#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        class VulkanTestRenderer : public Renderer {

        public:
            VulkanTestRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render();

            void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) final;

        private:
            Graphics::Shader* shader;
            Graphics::Buffer* buffer;

        };

    }

}

#endif
