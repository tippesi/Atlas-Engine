#include "ExampleRenderer.h"

#include "../loader/ShaderLoader.h"
#include "../loader/ModelImporter.h"
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
                // Setup of the render pass attachments
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

                // Define attachments, the pass is started with this description
                auto colorAttachment = Graphics::RenderingAttachment {
                    .image = colorImage,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .outputLayout = VK_IMAGE_LAYOUT_GENERAL,
                };
                auto colorAttachment2 = Graphics::RenderingAttachment {
                    .image = dummyImage,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .outputLayout = VK_IMAGE_LAYOUT_GENERAL,
                    .writeEnabled = false
                };
                auto depthAttachment = Graphics::RenderingAttachment {
                    .image = depthImage,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .outputLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                };
                mainRendering = Graphics::RenderingDesc {
                    .colorAttachments = { colorAttachment, colorAttachment2 },
                    .depthAttachment = depthAttachment
                };
            }
            {
                mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoader(
                    "sponza/sponza.obj", Atlas::Loader::ModelImporter::ImportMesh, false, 2048
                );

                auto shaderConfig = ShaderConfig {
                    {"example/mesh.vsh", VK_SHADER_STAGE_VERTEX_BIT},
                    {"example/mesh.fsh", VK_SHADER_STAGE_FRAGMENT_BIT},
                };
                auto pipelineDesc = Graphics::GraphicsPipelineDesc{
                    .attachmentFormats = Graphics::AttachmentFormats(mainRendering),
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
            
            GPUProfiler::BeginThread("Main thread", commandList);

            commandList->BeginCommands();

            GPUProfiler::BeginQuery("Main renderer");
            GPUProfiler::BeginQuery("Main model");

            {
                mainRendering.colorClearValue.color = {0.0f, 0.0f, blue, 1.0f};
                commandList->BeginRendering(mainRendering);

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
                    if (subData.material->baseColorMap.IsLoaded()) {
                        commandList->BindImage(baseColorTexture->image, baseColorTexture->sampler, 0, 1);
                    }

                    commandList->DrawIndexed(subData.indicesCount, 1, subData.indicesOffset);
                }

                commandList->EndRendering();
            }

            GPUProfiler::EndAndBeginQuery("Compute pass");

            {
                commandList->BindPipeline(computePipeline);

                auto randomConstants = ComputeConstants{
                    .randomSeed = Common::Random::SampleFastUniformFloat(),
                    .time = Clock::Get()
                };
                commandList->PushConstants("randomConstant", &randomConstants);

                commandList->BindImage(mainRendering.colorAttachments[0].image, 0, 0);
                commandList->Dispatch(1920 / 8, 1080 / 8, 1);
            }

            GPUProfiler::EndAndBeginQuery("Copy image");

            {
                Graphics::ImageBarrier imageBarriers[] = {
                    {destinationImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT},
                    {mainRendering.colorAttachments[0].image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT}
                };
                commandList->PipelineBarrier(imageBarriers, {},
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

                // Copy to other image
                commandList->CopyImage(mainRendering.colorAttachments[0].image, destinationImage);

                commandList->ImageMemoryBarrier(destinationImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            }

            GPUProfiler::EndAndBeginQuery("Post processing/render to swap chain");

            {
                swapChain->colorClearValue.color = {0.0f, 0.0f, blue, 1.0f};
                commandList->BeginRenderPass(swapChain, true);
                commandList->BindPipeline(pipeline);

                commandList->BindImage(destinationImage, linearSampler, 0, 0);

                commandList->Draw(6, 1, 0, 0);

                commandList->EndRenderPass();
            }

            GPUProfiler::EndQuery();
            GPUProfiler::EndQuery();
            GPUProfiler::EndThread();

            commandList->EndCommands();

            device->SubmitCommandList(commandList);

        }

    }

}
