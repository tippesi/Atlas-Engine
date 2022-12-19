#ifndef ATLASENGINE_VULKANTESTRENDERER_H
#define ATLASENGINE_VULKANTESTRENDERER_H

#include "Renderer.h"
#include "../mesh/VulkanMesh.h"

namespace Atlas {

    namespace Renderer {

        class VulkanTestRenderer : public Renderer {

        public:
            VulkanTestRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Camera* camera);

            void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) final;

        private:
            Graphics::Shader* shader = nullptr;
            Graphics::Shader* meshShader = nullptr;
            Graphics::Pipeline* pipeline = nullptr;
            Graphics::Pipeline* meshPipeline = nullptr;
            Graphics::Buffer* buffer = nullptr;

            Graphics::MultiBuffer* uniformBuffer = nullptr;

            Mesh::VulkanMesh* mesh = nullptr;

            struct PushConstants {
                mat4 vMatrix;
                mat4 pMatrix;
            };

            struct Uniforms {
                mat4 vMatrix;
                mat4 pMatrix;
            };

        };

    }

}

#endif
