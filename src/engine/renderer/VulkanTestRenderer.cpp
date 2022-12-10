#include "VulkanTestRenderer.h"

#include "../loader/ShaderLoader.h"

namespace Atlas {

    namespace Renderer {

        void VulkanTestRenderer::Init(Graphics::GraphicsDevice *device) {

            auto shaderDesc = Graphics::ShaderDesc {
                .vertexShaderStage = Loader::ShaderLoader::LoadFile("test.vsh", VK_SHADER_STAGE_VERTEX_BIT),
                .fragmentShaderStage = Loader::ShaderLoader::LoadFile("test.fsh", VK_SHADER_STAGE_FRAGMENT_BIT)
            };

            shaderDesc.vertexShaderStage.Compile();
            shaderDesc.fragmentShaderStage.Compile();

        }

        void VulkanTestRenderer::Render(Atlas::Viewport *viewport, Atlas::RenderTarget *target, Atlas::Camera *camera,
            Scene::Scene *scene) {



        }

    }

}