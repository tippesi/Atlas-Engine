#include "VulkanTestRenderer.h"

#include "../loader/ShaderLoader.h"
#include "../loader/ModelLoader.h"
#include "../common/RandomHelper.h"
#include "../Clock.h"

#include <thread>
#include <chrono>
#include <imgui.h>

namespace Atlas {

    namespace Renderer {

        VulkanTestRenderer::~VulkanTestRenderer() {

            delete mesh;

        }

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
                    .renderPass = device->swapChain->renderPass,
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
                auto colorImageDesc = Graphics::ImageDesc{
                    .usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                        | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                        | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                    .aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
                    .width = 1280,
                    .height = 720,
                    .format = VK_FORMAT_R8G8B8A8_UNORM
                };
                auto depthImageDesc = Graphics::ImageDesc{
                    .usageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    .aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT,
                    .width = 1280,
                    .height = 720,
                    .format = VK_FORMAT_D32_SFLOAT
                };
                auto colorImage = device->CreateImage(colorImageDesc);
                dstImage = device->CreateImage(colorImageDesc);
                auto depthImage = device->CreateImage(depthImageDesc);

                auto colorAttachment = Graphics::RenderPassAttachment{
                    .image = colorImage,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .outputLayout = VK_IMAGE_LAYOUT_GENERAL,
                };
                auto depthAttachment = Graphics::RenderPassAttachment{
                    .image = depthImage,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .outputLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                };
                auto renderPassDesc = Graphics::RenderPassDesc{
                    .colorAttachments = { colorAttachment },
                    .depthAttachment = depthAttachment,
                    .extent = { 1280, 720 }
                };
                mainRenderPass = device->CreateRenderPass(renderPassDesc);
                auto samplerDesc = Graphics::SamplerDesc{
                    .filter = VK_FILTER_LINEAR,
                    .mode = VK_SAMPLER_ADDRESS_MODE_REPEAT
                };
                mainRenderPassSampler = device->CreateSampler(samplerDesc);
            }
            {
                auto meshData = Loader::ModelLoader::LoadMesh("sponza/sponza.obj", false, glm::scale(glm::mat4(1.0f), glm::vec3(.05f)));
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
                    .renderPass = mainRenderPass->renderPass,
                    .shader = meshShader,
                    .vertexInputInfo = mesh->GetVertexInputState(),
                };
                meshPipeline = device->CreatePipeline(meshPipelineDesc);
            }

            {
                auto bufferDesc = Graphics::BufferDesc{
                    .usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    .domain = Graphics::BufferDomain::Host,
                    .size = sizeof(Uniforms)
                };
                uniformBuffer = device->CreateMultiBuffer(bufferDesc);
            }
            {
                auto computeStages = std::vector<Graphics::ShaderStageFile>{
                    Loader::ShaderLoader::LoadFile("test.csh", VK_SHADER_STAGE_COMPUTE_BIT)
                };
                auto computeShaderDesc = Graphics::ShaderDesc{
                    .stages = computeStages
                };
                computeShader = device->CreateShader(computeShaderDesc);
                auto pipelineDesc = Graphics::ComputePipelineDesc {
                    .shader = computeShader
                };
                computePipeline = device->CreatePipeline(pipelineDesc);
            }
            {
                renderPassToComputeBarrier = Graphics::ImageBarrier(VK_IMAGE_LAYOUT_GENERAL,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
                attachmentToTransferBarrier = Graphics::ImageBarrier(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
                dstToTransferBarrier = Graphics::ImageBarrier(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT);
                dstToShaderReadBarrier = Graphics::ImageBarrier(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT);
            }
        }

        void VulkanTestRenderer::Render(Camera* camera) {

            auto blue = abs(sin(Clock::Get()));

            auto commandList = device->GetCommandList(Atlas::Graphics::GraphicsQueue);
            auto swapChain = device->swapChain;

            commandList->BeginCommands();

            {
                mainRenderPass->colorClearValue.color = {0.0f, 0.0f, blue, 1.0f};
                commandList->BeginRenderPass(mainRenderPass, true);

                // Viewport and scissor are set to a default when binding a pipeline
                commandList->BindPipeline(meshPipeline);

                auto pushConstants = PushConstants{
                    .vMatrix = camera->viewMatrix,
                    .pMatrix = camera->projectionMatrix
                };

                auto uniforms = Uniforms{
                    .vMatrix = camera->viewMatrix,
                    .pMatrix = camera->projectionMatrix
                };
                uniformBuffer->SetData(&uniforms, 0, sizeof(Uniforms));

                auto pushConstantRange = meshShader->GetPushConstantRange("constants");
                commandList->PushConstants(pushConstantRange, &pushConstants);

                commandList->BindVertexBuffer(&mesh->vertexBuffer);
                commandList->BindVertexBuffer(&mesh->normalBuffer);
                commandList->BindVertexBuffer(&mesh->texCoordBuffer);
                commandList->BindVertexBuffer(&mesh->tangentBuffer);
                commandList->BindIndexBuffer(&mesh->indexBuffer);

                commandList->BindBuffer(uniformBuffer, 0, 0);

                for (auto &subData: mesh->data.subData) {
                    auto baseColorTexture = subData.vulkanMaterial->baseColorMap;
                    if (baseColorTexture) {
                        commandList->BindImage(baseColorTexture->image, baseColorTexture->sampler, 0, 1);
                    }

                    commandList->DrawIndexed(subData.indicesCount, 1, subData.indicesOffset);
                }

                commandList->EndRenderPass();
            }

            {
                renderPassToComputeBarrier.Update(mainRenderPass->GetColorImage(0));
                commandList->ImageMemoryBarrier(renderPassToComputeBarrier,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

                commandList->BindPipeline(computePipeline);

                auto randomPushConstants = computeShader->GetPushConstantRange("randomConstant");
                auto randomConstants = ComputeConstants{
                    .randomSeed = Common::Random::SampleFastUniformFloat(),
                    .time = Clock::Get()
                };
                commandList->PushConstants(randomPushConstants, &randomConstants);

                commandList->BindImage(mainRenderPass->GetColorImage(0), 0, 0);
                commandList->Dispatch(1280 / 8, 720 / 8, 1);
            }

            {
                auto imageBarriers = std::vector<Graphics::ImageBarrier>{ dstToTransferBarrier.Update(dstImage),
                    attachmentToTransferBarrier.Update(mainRenderPass->GetColorImage(0)) };
                auto bufferBarriers = std::vector<Graphics::BufferBarrier>();
                commandList->PipelineBarrier(imageBarriers, bufferBarriers,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

                // Copy to other image
                commandList->CopyImage(mainRenderPass->GetColorImage(0), dstImage);

                // Other image needs transition to be read optimal
                dstToShaderReadBarrier.Update(dstImage);
                commandList->ImageMemoryBarrier(dstToShaderReadBarrier, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            }

            {
                swapChain->colorClearValue.color = {0.0f, 0.0f, blue, 1.0f};
                commandList->BeginRenderPass(swapChain, true);
                commandList->BindPipeline(pipeline);

                commandList->BindImage(dstImage, mainRenderPassSampler, 0, 0);

                commandList->Draw(6, 1, 0, 0);

                commandList->EndRenderPass();
            }

            commandList->EndCommands();

            device->SubmitCommandList(commandList);

        }

        void VulkanTestRenderer::Render(Atlas::Viewport *viewport, Atlas::RenderTarget *target, Atlas::Camera *camera,
            Scene::Scene *scene) {



        }

    }

}