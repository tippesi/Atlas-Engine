#include "ExampleRenderer.h"

#include "../loader/ShaderLoader.h"
#include "../loader/ModelLoader.h"
#include "../resource/ResourceManager.h"
#include "../common/RandomHelper.h"
#include "../pipeline/PipelineManager.h"
#include "../Clock.h"

#include <thread>
#include <chrono>
#include <imgui.h>

namespace Atlas {

    namespace Renderer {

        void ExampleRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

            {
                // Example for pipeline setup
                auto shaderConfig = ShaderConfig {
                    {"example/triangle.vsh", VK_SHADER_STAGE_VERTEX_BIT},
                    {"example/triangle.fsh", VK_SHADER_STAGE_FRAGMENT_BIT},
                };
                auto pipelineDesc = Graphics::GraphicsPipelineDesc {
                    .swapChain = device->swapChain,
                    .rasterizer = Graphics::Initializers::InitPipelineRasterizationStateCreateInfo
                        (VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE)
                };
                auto pipelineConfig = PipelineConfig(shaderConfig, pipelineDesc);
                pipeline = PipelineManager::GetPipeline(pipelineConfig);
            }
            {
                // Vertex buffer example/buffer usage example
                int data = 5;
                auto bufferDesc = Graphics::BufferDesc {
                    .usageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    .data = &data,
                    .size = sizeof(int)
                };
                buffer = device->CreateBuffer(bufferDesc);

                int data2 = 10;
                bufferDesc = Graphics::BufferDesc {
                    .usageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    .data = &data2,
                    .size = sizeof(int)
                };
                buffer = device->CreateBuffer(bufferDesc);
            }
            {
                // Setup of render pass plus framebuffer
                auto colorImageDesc = Graphics::ImageDesc {
                    .usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                                  | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                                  | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                    .aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
                    .width = 1920,
                    .height = 1080,
                    .format = VK_FORMAT_R8G8B8A8_UNORM
                };
                auto depthImageDesc = Graphics::ImageDesc {
                    .usageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    .aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT,
                    .width = 1920,
                    .height = 1080,
                    .format = VK_FORMAT_D32_SFLOAT
                };

                // Create images for attachments
                auto colorImage = device->CreateImage(colorImageDesc);
                auto dummyImage = device->CreateImage(colorImageDesc);
                destinationImage = device->CreateImage(colorImageDesc);
                auto depthImage = device->CreateImage(depthImageDesc);

                // Define attachments
                auto colorAttachment = Graphics::RenderPassColorAttachment {
                    .imageFormat = colorImage->format,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .outputLayout = VK_IMAGE_LAYOUT_GENERAL,
                };
                auto colorAttachment2 = Graphics::RenderPassColorAttachment{
                    .imageFormat = dummyImage->format,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .outputLayout = VK_IMAGE_LAYOUT_GENERAL,
                };
                auto depthAttachment = Graphics::RenderPassDepthAttachment{
                    .imageFormat = depthImage->format,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .outputLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                };
                auto renderPassDesc = Graphics::RenderPassDesc {
                    .colorAttachments = { colorAttachment, colorAttachment2 },
                    .depthAttachment = depthAttachment
                };

                // Finally, create render pass
                mainRenderPass = device->CreateRenderPass(renderPassDesc);

                // Create frame buffer which references render pass
                auto frameBufferDesc = Graphics::FrameBufferDesc {
                    .renderPass = mainRenderPass,
                    .colorAttachments = {
                        { colorImage, 0, true },
                        { dummyImage, 0, false }
                    },
                    .depthAttachment = { depthImage, 0, true },
                    .extent = {1920, 1080}
                };
                mainFrameBuffer = device->CreateFrameBuffer(frameBufferDesc);
            }
            {
                mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoader(
                    "sponza/sponza.obj", Atlas::Loader::ModelLoader::LoadMesh, false, glm::mat4(1.0f), 2048
                );

                auto shaderConfig = ShaderConfig {
                    {"example/mesh.vsh", VK_SHADER_STAGE_VERTEX_BIT},
                    {"example/mesh.fsh", VK_SHADER_STAGE_FRAGMENT_BIT},
                };
                auto pipelineDesc = Graphics::GraphicsPipelineDesc{
                    .frameBuffer = mainFrameBuffer,
                    .vertexInputInfo = mesh->vertexArray.GetVertexInputState(),
                };
                auto pipelineConfig = PipelineConfig(shaderConfig, pipelineDesc, { "MYMACRO" });
                meshPipeline = PipelineManager::GetPipeline(pipelineConfig);
            }
            {
                // Uniform buffer usage
                auto bufferDesc = Graphics::BufferDesc{
                    .usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    .domain = Graphics::BufferDomain::Host,
                    .size = sizeof(Uniforms)
                };
                uniformBuffer = device->CreateMultiBuffer(bufferDesc);
            }
            {
                // Compute pipeline usage
                auto pipelineConfig = PipelineConfig("example/compute.csh");
                computePipeline = PipelineManager::GetPipeline(pipelineConfig);
            }
            {
                // Create linear sampler
                auto samplerDesc = Graphics::SamplerDesc {
                    .filter = VK_FILTER_LINEAR,
                    .mode = VK_SAMPLER_ADDRESS_MODE_REPEAT
                };
                linearSampler = device->CreateSampler(samplerDesc);
            }
            {
                // Query pool usage
                auto queryPoolDesc = Graphics::QueryPoolDesc {
                    .queryType = VK_QUERY_TYPE_TIMESTAMP,
                    .queryCount = 1000
                };
                queryPool = device->CreateQueryPool(queryPoolDesc);
            }
        }

        void ExampleRenderer::Render(const CameraComponent& camera) {

            auto swapChain = device->swapChain;
            if (!swapChain->isComplete) return;

            auto blue = abs(sin(Clock::Get()));

            auto commandList = device->GetCommandList(Graphics::GraphicsQueue);
            
            Graphics::Profiler::BeginThread("Main thread", commandList);

            commandList->BeginCommands();

            Graphics::Profiler::BeginQuery("Main renderer");
            Graphics::Profiler::BeginQuery("Main model");

            mainFrameBuffer->Refresh();

            {
                mainRenderPass->colorClearValue.color = {0.0f, 0.0f, blue, 1.0f};
                commandList->BeginRenderPass(mainRenderPass, mainFrameBuffer, true);

                // Viewport and scissor are set to a default when binding a pipeline
                commandList->BindPipeline(meshPipeline);

                auto pushConstants = PushConstants {
                    .vMatrix = camera.viewMatrix,
                    .pMatrix = camera.projectionMatrix
                };

                auto uniforms = Uniforms{
                    .vMatrix = camera.viewMatrix,
                    .pMatrix = camera.projectionMatrix
                };
                uniformBuffer->SetData(&uniforms, 0, sizeof(Uniforms));

                commandList->PushConstants("constants", &pushConstants);

                mesh->vertexArray.Bind(commandList);

                commandList->BindBuffer(uniformBuffer, 0, 0);

                for (auto &subData: mesh->data.subData) {
                    auto baseColorTexture = subData.material->baseColorMap;
                    if (baseColorTexture) {
                        commandList->BindImage(baseColorTexture->image, baseColorTexture->sampler, 0, 1);
                    }

                    commandList->DrawIndexed(subData.indicesCount, 1, subData.indicesOffset);
                }

                commandList->EndRenderPass();
            }

            Graphics::Profiler::EndAndBeginQuery("Compute pass");

            {
                commandList->BindPipeline(computePipeline);

                auto randomConstants = ComputeConstants{
                    .randomSeed = Common::Random::SampleFastUniformFloat(),
                    .time = Clock::Get()
                };
                commandList->PushConstants("randomConstant", &randomConstants);

                commandList->BindImage(mainFrameBuffer->GetColorImage(0), 0, 0);
                commandList->Dispatch(1920 / 8, 1080 / 8, 1);
            }

            Graphics::Profiler::EndAndBeginQuery("Copy image");

            {
                auto imageBarriers = std::vector<Graphics::ImageBarrier> {
                    {destinationImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT},
                    {mainFrameBuffer->GetColorImage(0), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT}
                };
                auto bufferBarriers = std::vector<Graphics::BufferBarrier>();
                commandList->PipelineBarrier(imageBarriers, bufferBarriers,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

                // Copy to other image
                commandList->CopyImage(mainFrameBuffer->GetColorImage(0), destinationImage);

                commandList->ImageMemoryBarrier(destinationImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            }

            Graphics::Profiler::EndAndBeginQuery("Post processing/render to swap chain");

            {
                swapChain->colorClearValue.color = {0.0f, 0.0f, blue, 1.0f};
                commandList->BeginRenderPass(swapChain, true);
                commandList->BindPipeline(pipeline);

                commandList->BindImage(destinationImage, linearSampler, 0, 0);

                commandList->Draw(6, 1, 0, 0);

                commandList->EndRenderPass();
            }

            Graphics::Profiler::EndQuery();
            Graphics::Profiler::EndQuery();
            Graphics::Profiler::EndThread();

            commandList->EndCommands();

            device->SubmitCommandList(commandList);

        }

    }

}
