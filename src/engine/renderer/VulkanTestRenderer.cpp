#include "VulkanTestRenderer.h"

#include "../loader/ShaderLoader.h"

namespace Atlas {

    namespace Renderer {

        void VulkanTestRenderer::Init(Graphics::GraphicsDevice *device) {

            auto stages = std::vector<Graphics::ShaderStageFile> {
                Loader::ShaderLoader::LoadFile("test.vsh", VK_SHADER_STAGE_VERTEX_BIT),
                Loader::ShaderLoader::LoadFile("test.fsh", VK_SHADER_STAGE_FRAGMENT_BIT)
            };
            auto shaderDesc = Graphics::ShaderDesc {
                .stages = stages
            };

            shader = device->CreateShader(shaderDesc);

        }

        void VulkanTestRenderer::Render(Atlas::Viewport *viewport, Atlas::RenderTarget *target, Atlas::Camera *camera,
            Scene::Scene *scene) {



        }

    }

}