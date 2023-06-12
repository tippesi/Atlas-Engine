#include "ImpostorRenderer.h"
#include "helper/GeometryHelper.h"

namespace Atlas {

    namespace Renderer {

        ImpostorRenderer::ImpostorRenderer() {

            /*
            Helper::GeometryHelper::GenerateRectangleVertexArray(vertexArray);

            shaderBatch.AddStage(AE_VERTEX_STAGE, "impostor/impostor.vsh");
            shaderBatch.AddStage(AE_FRAGMENT_STAGE, "impostor/impostor.fsh");

            interpolationConfig.AddMacro("INTERPOLATION");

            shaderBatch.AddConfig(&normalConfig);
            shaderBatch.AddConfig(&interpolationConfig);

            GetUniforms();
             */

        }

        void ImpostorRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, 
            RenderList* renderList, std::unordered_map<void*, uint16_t> materialMap) {

            /*
            Profiler::BeginQuery("Impostors");

            glDisable(GL_CULL_FACE);

            vertexArray.Bind();

            for (uint8_t i = 0; i < 2; i++) {

                switch (i) {
                case 0: shaderBatch.Bind(&normalConfig); break;
                case 1: shaderBatch.Bind(&interpolationConfig); break;
                default: break;
                }

                cameraRight->SetValue(camera->right);
                cameraUp->SetValue(camera->up);

                vMatrix->SetValue(camera->viewMatrix);
                pMatrix->SetValue(camera->projectionMatrix);
                cameraLocation->SetValue(camera->GetLocation());

                pvMatrixLast->SetValue(camera->GetLastJitteredMatrix());
                jitterLast->SetValue(camera->GetLastJitter());
                jitterCurrent->SetValue(camera->GetJitter());

                for (auto& key : renderList->impostorBuffers) {

                    auto mesh = key.first;
                    auto buffer = key.second;

                    // If there aren't any impostors there won't be a buffer
                    if (!buffer)
                        continue;

                    if (!mesh->impostor->interpolation)
                        continue;

                    auto actorCount = buffer->GetElementCount();

                    mesh->impostor->baseColorTexture.Bind(0);
                    mesh->impostor->roughnessMetalnessAoTexture.Bind(1);
                    mesh->impostor->normalTexture.Bind(2);

                    // Base 0 is used by the materials
                    mesh->impostor->viewPlaneBuffer.BindBase(1);
                    buffer->BindBase(2);

                    center->SetValue(mesh->impostor->center);
                    radius->SetValue(mesh->impostor->radius);

                    views->SetValue(mesh->impostor->views);
                    cutoff->SetValue(mesh->impostor->cutoff);
                    materialIdx->SetValue((uint32_t)materialMap[mesh->impostor]);

                    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)actorCount);

                }

            }

            glEnable(GL_CULL_FACE);

            Profiler::EndQuery();
             */

        }

        void ImpostorRenderer::Generate(Atlas::Viewport *viewport, const std::vector<mat4> &viewMatrices,
            glm::mat4 projectionMatrix, Mesh::Mesh *mesh, Mesh::Impostor *impostor) {

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
            uniformBuffer.SetData(&uniforms, 0, 1);
            uniformBuffer.Bind(commandList, 3, 7);

            mesh->vertexArray.Bind(commandList);

            for (size_t i = 0; i < viewMatrices.size(); i++) {

                frameBuffer->ChangeColorAttachmentImage(impostor->baseColorTexture.image, i, 0);
                frameBuffer->ChangeColorAttachmentImage(impostor->roughnessMetalnessAoTexture.image, i, 1);
                frameBuffer->ChangeColorAttachmentImage(impostor->normalTexture.image, i, 2);
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
                    };
                    commandList->PushConstants("constants", &pushConstants);

                    commandList->DrawIndexed(subData->indicesCount, 1, subData->indicesOffset, 0, 0);

                }

                commandList->EndRenderPass();

            }

            commandList->EndCommands();

            graphicsDevice->FlushCommandList(commandList);

        }

        void ImpostorRenderer::GetUniforms() {

            /*
            pMatrix = shaderBatch.GetUniform("pMatrix");
            vMatrix = shaderBatch.GetUniform("vMatrix");
            cameraLocation = shaderBatch.GetUniform("cameraLocation");

            center = shaderBatch.GetUniform("center");
            radius = shaderBatch.GetUniform("radius");

            cameraRight = shaderBatch.GetUniform("cameraRight");
            cameraUp = shaderBatch.GetUniform("cameraUp");

            views = shaderBatch.GetUniform("views");
            cutoff = shaderBatch.GetUniform("cutoff");
            materialIdx = shaderBatch.GetUniform("materialIdx");

            pvMatrixLast = shaderBatch.GetUniform("pvMatrixLast");
            jitterLast = shaderBatch.GetUniform("jitterLast");
            jitterCurrent = shaderBatch.GetUniform("jitterCurrent");
             */

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

            Graphics::RenderPassAttachment attachments[] = {
                {.imageFormat = VK_FORMAT_R8G8B8A8_UNORM},
                {.imageFormat = VK_FORMAT_R8G8B8A8_UNORM},
                {.imageFormat = VK_FORMAT_R8G8B8A8_UNORM},
                {.imageFormat = VK_FORMAT_D32_SFLOAT}
            };

            for (auto &attachment : attachments) {
                attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                attachment.outputLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }

            auto renderPassDesc = Graphics::RenderPassDesc{
                .colorAttachments = {attachments[0], attachments[1], attachments[2]},
                .depthAttachment = {attachments[3]}
            };
            auto renderPass = graphicsDevice->CreateRenderPass(renderPassDesc);

            auto frameBufferDesc = Graphics::FrameBufferDesc{
                .renderPass = renderPass,
                .colorAttachments = {
                    {impostor->baseColorTexture.image, 0, true},
                    {impostor->roughnessMetalnessAoTexture.image, 0, true},
                    {impostor->normalTexture.image, 0, true}
                },
                .depthAttachment = {depthImage, 0, true},
                .extent = {uint32_t(impostor->resolution), uint32_t(impostor->resolution)}
            };
            return graphicsDevice->CreateFrameBuffer(frameBufferDesc);

        }

        PipelineConfig ImpostorRenderer:

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