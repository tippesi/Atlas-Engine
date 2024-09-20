#include "ImpostorRenderer.h"
#include "helper/GeometryHelper.h"

namespace Atlas {

    namespace Renderer {

        void ImpostorRenderer::Init(Graphics::GraphicsDevice* device) {

            this->device = device;

            Helper::GeometryHelper::GenerateRectangleVertexArray(vertexArray);

        }

        void ImpostorRenderer::Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene,
            Graphics::CommandList* commandList, RenderList* renderList,
            std::unordered_map<void*, uint16_t> materialMap) {

            struct alignas(16) PushConstants {
                uint32_t materialIdx;
            };

            Graphics::Profiler::BeginQuery("Impostors");

            auto mainPass = renderList->GetMainPass();

            vertexArray.Bind(commandList);

            for (auto& item : mainPass->meshToInstancesMap) {

                auto meshId = item.first;
                auto instance = item.second;

                auto mesh = renderList->meshIdToMeshMap[meshId];

                // If there aren't any impostors there won't be a buffer
                if (!instance.impostorCount)
                    continue;

                auto config = GetPipelineConfig(target->gBufferFrameBuffer, mesh->impostor->interpolation, mesh->impostor->pixelDepthOffset);
                auto pipeline = PipelineManager::GetPipeline(config);

                commandList->BindPipeline(pipeline);

                mesh->impostor->baseColorTexture.Bind(commandList, 3, 0);
                mesh->impostor->roughnessMetalnessAoTexture.Bind(commandList, 3, 1);
                mesh->impostor->normalTexture.Bind(commandList, 3, 2);
                mesh->impostor->depthTexture.Bind(commandList, 3, 3);

                // Base 0 is used by the materials
                mesh->impostor->viewPlaneBuffer.Bind(commandList, 3, 4);
                mesh->impostor->impostorInfoBuffer.Bind(commandList, 3, 5);

                PushConstants constants = {
                    .materialIdx = uint32_t(materialMap[mesh->impostor.get()]),
                };
                commandList->PushConstants("constants", &constants);

                commandList->Draw(4, instance.impostorCount, 0, instance.impostorOffset);
            }

            Graphics::Profiler::EndQuery();

        }

        void ImpostorRenderer::Generate(const std::vector<mat4> &viewMatrices,
            glm::mat4 projectionMatrix, float distToCenter, Mesh::Mesh *mesh, Mesh::Impostor *impostor) {

            struct alignas(16) PushConstants {
                mat4 vMatrix = mat4(1.0f);
                vec4 baseColor = vec4(1.0f);
                float roughness = 1.0f;
                float metalness = 1.0f;
                float ao = 1.0f;
                uint32_t invertUVs = 0;
                uint32_t twoSided = 0;
                float normalScale = 1.0f;
                float displacementScale = 1.0f;
                float distanceToPlaneCenter = 1.0f;
            };

            struct Uniforms {
                mat4 pMatrix = mat4(1.0f);
            };

            auto graphicsDevice = Graphics::GraphicsDevice::DefaultDevice;
            auto commandList = graphicsDevice->GetCommandList(Graphics::GraphicsQueue, true);

            commandList->BeginCommands();

            auto frameBuffer = GenerateFrameBuffer(impostor);

            Buffer::UniformBuffer uniformBuffer(sizeof(Uniforms));
            Uniforms uniforms = {
                .pMatrix = projectionMatrix
            };
            uniformBuffer.SetData(&uniforms, 0);
            uniformBuffer.Bind(commandList, 3, 7);

            mesh->vertexArray.Bind(commandList);

            {
                // Transfer all framebuffer images including all mips into same layout/access as end of render pass
                std::vector<Graphics::ImageBarrier> imageBarriers;
                std::vector<Graphics::BufferBarrier> bufferBarriers;

                VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                VkAccessFlags access = VK_ACCESS_SHADER_READ_BIT;
                imageBarriers = {
                    {impostor->baseColorTexture.image,            layout, access},
                    {impostor->normalTexture.image,               layout, access},
                    {impostor->roughnessMetalnessAoTexture.image, layout, access},
                    {impostor->depthTexture.image, layout, access},
                    {frameBuffer->GetDepthImage(), layout, access},
                };
                commandList->PipelineBarrier(imageBarriers, bufferBarriers,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
            }

            for (size_t i = 0; i < viewMatrices.size(); i++) {

                frameBuffer->ChangeColorAttachmentImage(impostor->baseColorTexture.image, i, 0);
                frameBuffer->ChangeColorAttachmentImage(impostor->normalTexture.image, i, 1);
                frameBuffer->ChangeColorAttachmentImage(impostor->roughnessMetalnessAoTexture.image, i, 2);
                frameBuffer->ChangeColorAttachmentImage(impostor->depthTexture.image, i, 3);
                frameBuffer->Refresh();

                commandList->BeginRenderPass(frameBuffer->renderPass, frameBuffer, true);

                for (size_t j = 0; j < mesh->data.subData.size(); j++) {

                    auto subData = &mesh->data.subData[j];
                    auto material = subData->material;
                    auto config = GetPipelineConfigForSubData(subData, mesh, frameBuffer);
                    auto pipeline = PipelineManager::GetPipeline(config);

                    commandList->BindPipeline(pipeline);

                    if (material->HasBaseColorMap())
                        commandList->BindImage(material->baseColorMap->image, material->baseColorMap->sampler, 3, 0);
                    if (material->HasOpacityMap())
                        commandList->BindImage(material->opacityMap->image, material->opacityMap->sampler, 3, 1);
                    if (material->HasNormalMap())
                        commandList->BindImage(material->normalMap->image, material->normalMap->sampler, 3, 2);
                    if (material->HasRoughnessMap())
                        commandList->BindImage(material->roughnessMap->image, material->roughnessMap->sampler, 3, 3);
                    if (material->HasMetalnessMap())
                        commandList->BindImage(material->metalnessMap->image, material->metalnessMap->sampler, 3, 4);
                    if (material->HasAoMap())
                        commandList->BindImage(material->aoMap->image, material->aoMap->sampler, 3, 5);
                    if (material->HasDisplacementMap())
                        commandList->BindImage(material->displacementMap->image, material->displacementMap->sampler, 3, 6);

                    auto pushConstants = PushConstants {
                        .vMatrix = viewMatrices[i],
                        .baseColor = vec4(material->baseColor, 1.0f),
                        .roughness = material->roughness,
                        .metalness = material->metalness,
                        .ao = material->ao,
                        .invertUVs = mesh->invertUVs ? 1u : 0u,
                        .twoSided = material->twoSided ? 1u : 0u,
                        .normalScale = material->normalScale,
                        .displacementScale = material->displacementScale,
                        .distanceToPlaneCenter = distToCenter
                    };
                    commandList->PushConstants("constants", &pushConstants);

                    commandList->DrawIndexed(subData->indicesCount, 1, subData->indicesOffset, 0, 0);

                }

                commandList->EndRenderPass();

            }

            {
                std::vector<Graphics::ImageBarrier> imageBarriers;
                std::vector<Graphics::BufferBarrier> bufferBarriers;

                VkImageLayout layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                VkAccessFlags access = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
                imageBarriers = {
                    {impostor->baseColorTexture.image, layout, access},
                    {impostor->normalTexture.image, layout, access},
                    {impostor->roughnessMetalnessAoTexture.image, layout, access},
                    {impostor->depthTexture.image, layout, access},
                };
                commandList->PipelineBarrier(imageBarriers, bufferBarriers,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

                commandList->GenerateMipMaps(impostor->baseColorTexture.image);
                commandList->GenerateMipMaps(impostor->normalTexture.image);
                commandList->GenerateMipMaps(impostor->roughnessMetalnessAoTexture.image);
                commandList->GenerateMipMaps(impostor->depthTexture.image);

                imageBarriers = {
                    {impostor->baseColorTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {impostor->normalTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {impostor->roughnessMetalnessAoTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {impostor->depthTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                };
                commandList->PipelineBarrier(imageBarriers, bufferBarriers,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
            }

            commandList->EndCommands();

            graphicsDevice->FlushCommandList(commandList);

        }

        Ref<Graphics::FrameBuffer> ImpostorRenderer::GenerateFrameBuffer(Mesh::Impostor *impostor) {

            auto graphicsDevice = Graphics::GraphicsDevice::DefaultDevice;

            Graphics::ImageDesc imageDesc = {
                .usageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                .aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT,
                .width = uint32_t(impostor->resolution),
                .height = uint32_t(impostor->resolution),
                .format = VK_FORMAT_D32_SFLOAT
            };
            auto depthImage = graphicsDevice->CreateImage(imageDesc);

            Graphics::RenderPassColorAttachment colorAttachments[] = {
                {.imageFormat = VK_FORMAT_R8G8B8A8_UNORM},
                {.imageFormat = VK_FORMAT_R8G8B8A8_UNORM},
                {.imageFormat = VK_FORMAT_R8G8B8A8_UNORM},
                {.imageFormat = VK_FORMAT_R16_SFLOAT}
            };

            for (auto &attachment : colorAttachments) {
                attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                attachment.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                attachment.outputLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }

            Graphics::RenderPassDepthAttachment depthAttachment = {
                .imageFormat = VK_FORMAT_D32_SFLOAT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .outputLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            };

            auto renderPassDesc = Graphics::RenderPassDesc{
                .colorAttachments = {colorAttachments[0], colorAttachments[1], colorAttachments[2], colorAttachments[3]},
                .depthAttachment = depthAttachment,
                .colorClearValue = { .color = { { 0.0f, 0.0f, 0.0f, 0.0f } } },
            };
            auto renderPass = graphicsDevice->CreateRenderPass(renderPassDesc);

            auto frameBufferDesc = Graphics::FrameBufferDesc{
                .renderPass = renderPass,
                .colorAttachments = {
                    {impostor->baseColorTexture.image, 0, true},
                    {impostor->normalTexture.image, 0, true},
                    {impostor->roughnessMetalnessAoTexture.image, 0, true},
                    {impostor->depthTexture.image, 0, true}
                },
                .depthAttachment = {depthImage, 0, true},
                .extent = {uint32_t(impostor->resolution), uint32_t(impostor->resolution)}
            };
            return graphicsDevice->CreateFrameBuffer(frameBufferDesc);

        }

        PipelineConfig ImpostorRenderer::GetPipelineConfig(Ref<Graphics::FrameBuffer> &frameBuffer,
            bool interpolation, bool pixelDepthOffset) {

            auto shaderConfig = ShaderConfig {
                {"impostor/impostor.vsh", VK_SHADER_STAGE_VERTEX_BIT},
                {"impostor/impostor.fsh", VK_SHADER_STAGE_FRAGMENT_BIT},
            };
            auto pipelineDesc = Graphics::GraphicsPipelineDesc{
                .frameBuffer = frameBuffer,
                .vertexInputInfo = vertexArray.GetVertexInputState(),
            };

            pipelineDesc.assemblyInputInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            pipelineDesc.rasterizer.cullMode = VK_CULL_MODE_NONE;

            std::vector<std::string> macros;
            if (interpolation) macros.push_back("INTERPOLATION");
            if (pixelDepthOffset) macros.push_back("PIXEL_DEPTH_OFFSET");

            return PipelineConfig(shaderConfig, pipelineDesc, macros);

        }

        PipelineConfig ImpostorRenderer::GetPipelineConfigForSubData(Mesh::MeshSubData *subData,
            Mesh::Mesh *mesh, Ref<Graphics::FrameBuffer>& frameBuffer) {

            auto material = subData->material;

            auto shaderConfig = ShaderConfig {
                {"impostor/generate.vsh", VK_SHADER_STAGE_VERTEX_BIT},
                {"impostor/generate.fsh", VK_SHADER_STAGE_FRAGMENT_BIT},
            };
            auto pipelineDesc = Graphics::GraphicsPipelineDesc{
                .frameBuffer = frameBuffer,
                .vertexInputInfo = mesh->vertexArray.GetVertexInputState(),
            };

            if (material->twoSided || !mesh->cullBackFaces) {
                pipelineDesc.rasterizer.cullMode = VK_CULL_MODE_NONE;
            }

            std::vector<std::string> macros;
            if (material->HasBaseColorMap()) {
                macros.push_back("BASE_COLOR_MAP");
            }
            if (material->HasOpacityMap()) {
                macros.push_back("OPACITY_MAP");
            }
            if (material->HasNormalMap()) {
                macros.push_back("NORMAL_MAP");
            }
            if (material->HasRoughnessMap()) {
                macros.push_back("ROUGHNESS_MAP");
            }
            if (material->HasMetalnessMap()) {
                macros.push_back("METALNESS_MAP");
            }
            if (material->HasAoMap()) {
                macros.push_back("AO_MAP");
            }
            if (material->HasDisplacementMap()) {
                macros.push_back("HEIGHT_MAP");
            }
            // This is a check if we have any maps at all (no macros, no maps)
            if (macros.size()) {
                macros.push_back("TEX_COORDS");
            }
            if (glm::length(material->emissiveColor) > 0.0f) {
                macros.push_back("EMISSIVE");
            }

            return PipelineConfig(shaderConfig, pipelineDesc, macros);

        }

    }

}