#include "VulkanTestRenderer.h"

#include "../loader/ShaderLoader.h"
#include "../Clock.h"

namespace Atlas {

    namespace Renderer {

        void VulkanTestRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;
            auto stages = std::vector<Graphics::ShaderStageFile> {
                Loader::ShaderLoader::LoadFile("test.vsh", VK_SHADER_STAGE_VERTEX_BIT),
                Loader::ShaderLoader::LoadFile("test.fsh", VK_SHADER_STAGE_FRAGMENT_BIT)
            };
            auto shaderDesc = Graphics::ShaderDesc {
                .stages = stages,
                .renderPass = device->swapChain->defaultRenderPass,
                .viewportWidth = device->swapChain->extent.width,
                .viewportHeight = device->swapChain->extent.height
            };

            shader = device->CreateShader(shaderDesc);

            int data = 5;
            auto bufferDesc = Graphics::BufferDesc {
                .usageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .data = &data,
                .size = sizeof(int)
            };
            buffer = device->CreateBuffer(bufferDesc);


        }

        void VulkanTestRenderer::Render() {

            auto blue = abs(sin(Clock::Get()));

            auto commandList = device->GetCommandList(Atlas::Graphics::GraphicsQueue);
            auto swapChain = device->swapChain;

            commandList->BeginCommands();

            swapChain->clearValue.color = { 0.0f, 0.0f, blue, 1.0f};
            commandList->BeginRenderPass(swapChain);

            vkCmdBindPipeline(commandList->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->pipeline);
            vkCmdDraw(commandList->commandBuffer, 3, 1, 0, 0);

            commandList->EndRenderPass();
            commandList->EndCommands();

            device->SubmitCommandList(commandList);
            device->CompleteFrame();

        }

        void VulkanTestRenderer::Render(Atlas::Viewport *viewport, Atlas::RenderTarget *target, Atlas::Camera *camera,
            Scene::Scene *scene) {



        }

    }

}