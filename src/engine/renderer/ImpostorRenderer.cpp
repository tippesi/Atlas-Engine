#include "ImpostorRenderer.h"
#include "helper/GeometryHelper.h"

namespace Atlas {

    namespace Renderer {

        void ImpostorRenderer::Init(Graphics::GraphicsDevice* device) {

            this->device = device;

            Helper::GeometryHelper::GenerateRectangleVertexArray(vertexArray);

        }

        void ImpostorRenderer::Render(const Ref<RenderTarget>& target, const Ref<Scene::Scene>& scene,
            Graphics::CommandList* commandList, RenderList* renderList,
            std::unordered_map<void*, uint16_t>& materialMap, bool lateDraws) {

            struct alignas(16) PushConstants {
                uint32_t materialIdx;
            };

            CPUProfiler::Scope profilerScope("Impostors");
            GPUProfiler::Scope gpuScope("Impostors");

            auto mainPass = renderList->GetMainPass();

            vertexArray.Bind(commandList);

            // The instances are read from the instance buffer, except the ones of skinned meshes. All impostors share
            // a few pipeline variants, a pipeline is only looked up and bound when the variant changes
            int32_t boundVariant = -1;
            const Graphics::PushConstantRange* constantsRange = nullptr;
            auto bindImpostor = [&](Mesh::Mesh* mesh, bool useInstanceBuffer) {
                auto& impostor = mesh->impostor;
                auto variant = int32_t(impostor->interpolation) | int32_t(impostor->pixelDepthOffset) << 1 | int32_t(useInstanceBuffer) << 2;
                if (variant != boundVariant) {
                    boundVariant = variant;
                    auto config = GetPipelineConfig(Graphics::AttachmentFormats(target->gBufferRendering),
                        impostor->interpolation, impostor->pixelDepthOffset, useInstanceBuffer);
                    auto pipeline = PipelineManager::GetPipeline(config);
                    commandList->BindPipeline(pipeline);
                    constantsRange = pipeline->shader->GetPushConstantRange("constants");
                }

                impostor->baseColorTexture->Bind(commandList, 3, 0);
                impostor->roughnessMetalnessAoTexture->Bind(commandList, 3, 1);
                impostor->normalTexture->Bind(commandList, 3, 2);
                impostor->depthTexture->Bind(commandList, 3, 3);

                // Base 0 is used by the materials
                impostor->viewPlaneBuffer.Bind(commandList, 3, 4);
                impostor->impostorInfoBuffer.Bind(commandList, 3, 5);

                auto material = materialMap.find(impostor.Get().get());
                PushConstants constants = {
                    .materialIdx = material != materialMap.end() ? uint32_t(material->second) : 0u,
                };
                commandList->PushConstants(constantsRange, &constants);
            };

            for (const auto& item : mainPass->meshToInstancesMap) {

                auto meshId = item.first;
                auto instance = item.second;

                // If there aren't any impostors there won't be a buffer
                if (!instance.impostorCount || lateDraws)
                    continue;

                const auto& mesh = renderList->meshIdToMeshMap[meshId];

                bindImpostor(mesh.Get().get(), !mesh->data.IsSkinned());
                commandList->Draw(4, instance.impostorCount, 0, instance.impostorOffset);
            }

            for (const auto& draw : lateDraws ? mainPass->gpuLateDraws : mainPass->gpuDraws) {
                if (draw.type != RenderList::GPUDrawType::Impostor)
                    continue;

                bindImpostor(draw.mesh, true);
                renderList->DrawGPUCulled(commandList, draw);
            }

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

            Graphics::ImageDesc depthImageDesc = {
                .usageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                .aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT,
                .width = uint32_t(impostor->resolution),
                .height = uint32_t(impostor->resolution),
                .format = VK_FORMAT_D32_SFLOAT
            };
            auto depthImage = graphicsDevice->CreateImage(depthImageDesc);

            Buffer::UniformBuffer uniformBuffer(sizeof(Uniforms));
            Uniforms uniforms = {
                .pMatrix = projectionMatrix
            };
            uniformBuffer.SetData(&uniforms, 0);
            uniformBuffer.Bind(commandList, 3, 7);

            mesh->vertexArray.Bind(commandList);

            {
                // Transfer all images including all mips into same layout/access as end of render pass

                VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                VkAccessFlags access = VK_ACCESS_SHADER_READ_BIT;
                Graphics::ImageBarrier imageBarriers[] = {
                    {impostor->baseColorTexture->image,            layout, access},
                    {impostor->normalTexture->image,               layout, access},
                    {impostor->roughnessMetalnessAoTexture->image, layout, access},
                    {impostor->depthTexture->image, layout, access},
                };
                commandList->PipelineBarrier(imageBarriers, {},
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
            }

            for (size_t i = 0; i < viewMatrices.size(); i++) {

                auto layer = uint32_t(i);
                auto clear = VK_ATTACHMENT_LOAD_OP_CLEAR;
                auto rendering = Graphics::RenderingDesc{
                    .colorAttachments = {
                        {.image = impostor->baseColorTexture->image, .layer = layer, .loadOp = clear},
                        {.image = impostor->normalTexture->image, .layer = layer, .loadOp = clear},
                        {.image = impostor->roughnessMetalnessAoTexture->image, .layer = layer, .loadOp = clear},
                        {.image = impostor->depthTexture->image, .layer = layer, .loadOp = clear}
                    },
                    .depthAttachment = {.image = depthImage, .loadOp = clear},
                    .colorClearValue = { .color = { { 0.0f, 0.0f, 0.0f, 0.0f } } },
                };
                commandList->BeginRendering(rendering);

                for (size_t j = 0; j < mesh->data.subData.size(); j++) {

                    auto subData = &mesh->data.subData[j];
                    auto material = subData->material;
                    auto config = GetPipelineConfigForSubData(subData, mesh, Graphics::AttachmentFormats(rendering));
                    auto pipeline = PipelineManager::GetPipeline(config);

                    commandList->BindPipeline(pipeline);

                    if (material->baseColorMap.IsLoaded())
                        commandList->BindImage(material->baseColorMap->image, material->baseColorMap->sampler, 3, 0);
                    if (material->opacityMap.IsLoaded())
                        commandList->BindImage(material->opacityMap->image, material->opacityMap->sampler, 3, 1);
                    if (material->normalMap.IsLoaded())
                        commandList->BindImage(material->normalMap->image, material->normalMap->sampler, 3, 2);
                    if (material->roughnessMap.IsLoaded())
                        commandList->BindImage(material->roughnessMap->image, material->roughnessMap->sampler, 3, 3);
                    if (material->metalnessMap.IsLoaded())
                        commandList->BindImage(material->metalnessMap->image, material->metalnessMap->sampler, 3, 4);
                    if (material->aoMap.IsLoaded())
                        commandList->BindImage(material->aoMap->image, material->aoMap->sampler, 3, 5);
                    if (material->displacementMap.IsLoaded())
                        commandList->BindImage(material->displacementMap->image, material->displacementMap->sampler, 3, 6);

                    auto pushConstants = PushConstants {
                        .vMatrix = viewMatrices[i],
                        .baseColor = vec4(Common::ColorConverter::ConvertSRGBToLinear(material->baseColor), 1.0f),
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

                commandList->EndRendering();

            }

            {
                VkImageLayout layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                VkAccessFlags access = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
                Graphics::ImageBarrier preImageBarriers[] = {
                    {impostor->baseColorTexture->image, layout, access},
                    {impostor->normalTexture->image, layout, access},
                    {impostor->roughnessMetalnessAoTexture->image, layout, access},
                    {impostor->depthTexture->image, layout, access},
                };
                commandList->PipelineBarrier(preImageBarriers, {},
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

                commandList->GenerateMipMaps(impostor->baseColorTexture->image);
                commandList->GenerateMipMaps(impostor->normalTexture->image);
                commandList->GenerateMipMaps(impostor->roughnessMetalnessAoTexture->image);
                commandList->GenerateMipMaps(impostor->depthTexture->image);

                Graphics::ImageBarrier postImageBarriers[] = {
                    {impostor->baseColorTexture->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {impostor->normalTexture->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {impostor->roughnessMetalnessAoTexture->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {impostor->depthTexture->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                };
                commandList->PipelineBarrier(postImageBarriers, {},
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
            }

            commandList->EndCommands();

            graphicsDevice->FlushCommandList(commandList);

            impostor->isGenerated = true;

        }

        PipelineConfig ImpostorRenderer::GetPipelineConfig(const Graphics::AttachmentFormats& attachmentFormats,
            bool interpolation, bool pixelDepthOffset, bool useInstanceBuffer) {

            auto shaderConfig = ShaderConfig {
                {"impostor/impostor.vsh", VK_SHADER_STAGE_VERTEX_BIT},
                {"impostor/impostor.fsh", VK_SHADER_STAGE_FRAGMENT_BIT},
            };
            auto pipelineDesc = Graphics::GraphicsPipelineDesc{
                .attachmentFormats = attachmentFormats,
                .vertexInputInfo = vertexArray.GetVertexInputState(),
            };

            pipelineDesc.assemblyInputInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            pipelineDesc.rasterizer.cullMode = VK_CULL_MODE_NONE;

            std::vector<std::string> macros;
            if (interpolation) macros.push_back("INTERPOLATION");
            if (pixelDepthOffset) macros.push_back("PIXEL_DEPTH_OFFSET");
            if (useInstanceBuffer) macros.push_back("INSTANCE_BUFFER");

            return PipelineConfig(shaderConfig, pipelineDesc, macros);

        }

        PipelineConfig ImpostorRenderer::GetPipelineConfigForSubData(const Mesh::MeshSubData *subData,
            Mesh::Mesh *mesh, const Graphics::AttachmentFormats& attachmentFormats) {

            auto material = subData->material;

            auto shaderConfig = ShaderConfig {
                {"impostor/generate.vsh", VK_SHADER_STAGE_VERTEX_BIT},
                {"impostor/generate.fsh", VK_SHADER_STAGE_FRAGMENT_BIT},
            };
            auto pipelineDesc = Graphics::GraphicsPipelineDesc{
                .attachmentFormats = attachmentFormats,
                .vertexInputInfo = mesh->vertexArray.GetVertexInputState(),
            };

            if (material->twoSided || !mesh->cullBackFaces) {
                pipelineDesc.rasterizer.cullMode = VK_CULL_MODE_NONE;
            }

            std::vector<std::string> macros;
            if (material->baseColorMap.IsLoaded()) {
                macros.push_back("BASE_COLOR_MAP");
            }
            if (material->opacityMap.IsLoaded()) {
                macros.push_back("OPACITY_MAP");
            }
            if (material->normalMap.IsLoaded()) {
                macros.push_back("NORMAL_MAP");
            }
            if (material->roughnessMap.IsLoaded()) {
                macros.push_back("ROUGHNESS_MAP");
            }
            if (material->metalnessMap.IsLoaded()) {
                macros.push_back("METALNESS_MAP");
            }
            if (material->aoMap.IsLoaded()) {
                macros.push_back("AO_MAP");
            }
            if (material->displacementMap.IsLoaded()) {
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
