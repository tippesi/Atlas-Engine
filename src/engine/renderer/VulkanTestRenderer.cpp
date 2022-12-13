#include "VulkanTestRenderer.h"

#include "../loader/ShaderLoader.h"
#include "../loader/ModelLoader.h"
#include "../Clock.h"

namespace Atlas {

    namespace Renderer {

        void VulkanTestRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

            {
                auto stages = std::vector<Graphics::ShaderStageFile>{
                    Loader::ShaderLoader::LoadFile("test.vsh", VK_SHADER_STAGE_VERTEX_BIT),
                    Loader::ShaderLoader::LoadFile("test.fsh", VK_SHADER_STAGE_FRAGMENT_BIT)
                };
                auto shaderDesc = Graphics::ShaderDesc{
                    .stages = stages
                };
                shader = device->CreateShader(shaderDesc);

                auto pipelineDesc = Graphics::GraphicsPipelineDesc{
                    .renderPass = device->swapChain->defaultRenderPass,
                    .shader = shader
                };
                pipeline = device->CreatePipeline(pipelineDesc);
            }

            {
                int data = 5;
                auto bufferDesc = Graphics::BufferDesc{
                    .usageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    .data = &data,
                    .size = sizeof(int)
                };
                buffer = device->CreateBuffer(bufferDesc);

                int data2 = 10;
                bufferDesc = Graphics::BufferDesc{
                    .usageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    .data = &data2,
                    .size = sizeof(int)
                };
                buffer = device->CreateBuffer(bufferDesc);
            }

            {
                auto meshData = Loader::ModelLoader::LoadMesh("sponza/sponza.obj");
                mesh = new Mesh::VulkanMesh(meshData);

                auto meshStages = std::vector<Graphics::ShaderStageFile>{
                    Loader::ShaderLoader::LoadFile("testmesh.vsh", VK_SHADER_STAGE_VERTEX_BIT),
                    Loader::ShaderLoader::LoadFile("testmesh.fsh", VK_SHADER_STAGE_FRAGMENT_BIT)
                };
                auto meshShaderDesc = Graphics::ShaderDesc{
                    .stages = meshStages
                };
                meshShader = device->CreateShader(meshShaderDesc);

                auto meshPipelineDesc = Graphics::GraphicsPipelineDesc{
                    .renderPass = device->swapChain->defaultRenderPass,
                    .shader = meshShader,
                    .vertexInputInfo = mesh->GetVertexInputState()
                };
                meshPipeline = device->CreatePipeline(meshPipelineDesc);
            }


        }

        void VulkanTestRenderer::Render(Camera* camera) {

            auto blue = abs(sin(Clock::Get()));

            auto commandList = device->GetCommandList(Atlas::Graphics::GraphicsQueue);
            auto swapChain = device->swapChain;

            commandList->BeginCommands();

            swapChain->colorClearValue.color = { 0.0f, 0.0f, blue, 1.0f};
            commandList->BeginRenderPass(swapChain);

            // Viewport and scissor are set to a default when binding a pipeline
            commandList->BindPipeline(meshPipeline);

            auto pushConstants = PushConstants {
                .vMatrix = camera->viewMatrix,
                .pMatrix = camera->projectionMatrix
            };

            auto pushConstantRange = meshShader->GetPushConstantRange("constants");
            commandList->PushConstants(pushConstantRange, &pushConstants);

            commandList->BindVertexBuffer(&mesh->vertexBuffer);
            commandList->BindVertexBuffer(&mesh->normalBuffer);
            commandList->BindVertexBuffer(&mesh->texCoordBuffer);
            commandList->BindVertexBuffer(&mesh->tangentBuffer);
            commandList->BindIndexBuffer(&mesh->indexBuffer);

            commandList->DrawIndexed(mesh->data.GetIndexCount());

            // TODO: Implement in command list
            // vkCmdDraw(commandList->commandBuffer, 3, 1, 0, 0);


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