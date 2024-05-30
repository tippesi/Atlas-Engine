#include "ShadowRenderer.h"

#include "../Clock.h"

namespace Atlas {

    namespace Renderer {

        void ShadowRenderer::Init(Graphics::GraphicsDevice* device) {

            this->device = device;

            impostorRenderer.Init(device);

        }

        void ShadowRenderer::Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList, RenderList* renderList) {

            Graphics::Profiler::BeginQuery("Shadows");

            auto lightSubset = scene->GetSubset<LightComponent>();

            LightMap usedLightMap;
            for (auto& lightEntity : lightSubset) {

                auto& light = lightEntity.GetComponent<LightComponent>();
                if (!light.shadow || !light.shadow->update)
                    continue;

                auto shadow = light.shadow;
                auto frameBuffer = GetOrCreateFrameBuffer(lightEntity);
                usedLightMap[lightEntity] = frameBuffer;

                if (frameBuffer->depthAttachment.layer != 0) {
                    frameBuffer->depthAttachment.layer = 0;
                    frameBuffer->Refresh();
                }

                // We don't want to render to the long range component if it exists
                auto componentCount = shadow->viewCount;

                bool isDirectionalLight = false;
                vec3 lightLocation;

                if (light.type == LightType::DirectionalLight) {
                    lightLocation = 1000000.0f * -normalize(light.transformedProperties.directional.direction);
                    isDirectionalLight = true;
                }
                else if (light.type == LightType::PointLight) {
                    lightLocation = light.transformedProperties.point.position;
                }

                for (uint32_t i = 0; i < uint32_t(componentCount); i++) {

                    auto component = &shadow->views[i];

                    if (frameBuffer->depthAttachment.layer != i) {
                        frameBuffer->depthAttachment.layer = i;
                        frameBuffer->Refresh();
                    }

                    auto shadowPass = renderList->GetShadowPass(lightEntity, i);
                    if (!shadowPass) {
                        // Need to go through render passes to make sure images have transitioned
                        commandList->BeginRenderPass(frameBuffer->renderPass, frameBuffer, true);
                        commandList->EndRenderPass();
                        continue;
                    }

                    commandList->BeginRenderPass(frameBuffer->renderPass, frameBuffer, true);

                    auto lightSpaceMatrix = component->projectionMatrix * component->viewMatrix;

                    // Bind wind map (need to bind for each render pass, since impostor renderer resets this binding)
                    scene->wind.noiseMap.Bind(commandList, 3, 1);

                    int32_t subDataCount = 0;
                    // Retrieve all possible materials
                    for (auto& [meshId, instances] : shadowPass->meshToInstancesMap) {
                        if (!instances.count) continue;

                        auto& mesh = shadowPass->meshIdToMeshMap[meshId];
                        for (auto& subData : mesh->data.subData) {
                            if (subDataCount < subDatas.size())
                                subDatas[subDataCount] = {&subData, mesh.GetID(), mesh.Get().get()};
                            else
                                subDatas.push_back({ &subData, mesh.GetID(), mesh.Get().get() });
                            subDataCount++;
                        }
                    }

                    // Check whether materials have pipeline configs
                    for (int32_t i = 0; i < subDataCount; i++) {
                        auto& [subData, _, mesh] = subDatas[i];
                        auto& material = subData->material;
                        if (material->shadowConfig.IsValid()) continue;

                        material->shadowConfig = GetPipelineConfigForSubData(subData, mesh, frameBuffer);
                    }

                    // Sort materials by hash
                    std::sort(subDatas.begin(), subDatas.begin() + size_t(subDataCount),
                        [](auto& subData0, auto& subData1) {
                            return std::get<0>(subData0)->material->shadowConfig.variantHash <
                                std::get<0>(subData1)->material->shadowConfig.variantHash;
                        });

                    Hash prevHash = 0;
                    Hash prevMesh = 0;
                    Ref<Graphics::Pipeline> currentPipeline = nullptr;
                    for (int32_t i = 0; i < subDataCount; i++) {
                        auto& [subData, meshID, mesh] = subDatas[i];
                        auto& instances = shadowPass->meshToInstancesMap[meshID];

                        auto material = subData->material;

                        // Overwrite material config frame buffer (and assume these have the same format)
                        material->shadowConfig.graphicsPipelineDesc.frameBuffer = frameBuffer;

                        if (material->shadowConfig.variantHash != prevHash) {
                            currentPipeline = PipelineManager::GetPipeline(material->shadowConfig);
                            commandList->BindPipeline(currentPipeline);
                            prevHash = material->shadowConfig.variantHash;
                        }

                        if (meshID != prevMesh) {
                            mesh->vertexArray.Bind(commandList);
                            prevMesh = meshID;
                        }                        

                        if (material->HasOpacityMap())
                            commandList->BindImage(material->opacityMap->image, material->opacityMap->sampler, 3, 0);

                        auto pushConstants = PushConstants {
                            .lightSpaceMatrix = lightSpaceMatrix,
                            .vegetation = mesh->vegetation ? 1u : 0u,
                            .invertUVs = mesh->invertUVs ? 1u : 0u,
                            .windTextureLod = mesh->windNoiseTextureLod,
                            .windBendScale = mesh->windBendScale,
                            .windWiggleScale = mesh->windWiggleScale
                        };
                        commandList->PushConstants("constants", &pushConstants);

                        commandList->DrawIndexed(subData->indicesCount, instances.count, subData->indicesOffset,
                            0, instances.offset);

                        // Reset the frame buffer here to let it be able to be deleted
                        material->shadowConfig.graphicsPipelineDesc.frameBuffer = nullptr;

                    }

                    impostorRenderer.Render(frameBuffer, renderList, commandList,
                        shadowPass.get(), component->viewMatrix, component->projectionMatrix, lightLocation);

                    commandList->EndRenderPass();

                }

            }

            lightMap = usedLightMap;

            Graphics::Profiler::EndQuery();

        }

        Ref<Graphics::FrameBuffer> ShadowRenderer::GetOrCreateFrameBuffer(Scene::Entity entity) {

            auto& light = entity.GetComponent<LightComponent>();
            auto& shadow = light.shadow;
            
            /*
            if (lightMap.contains(entity)) {
                auto frameBuffer = lightMap[entity];
                if (frameBuffer->extent.width == shadow->resolution ||
                    frameBuffer->extent.height == shadow->resolution) {
                    return frameBuffer;
                }
            }
            */

            Graphics::RenderPassDepthAttachment attachment = {
                .imageFormat = shadow->useCubemap ? shadow->cubemap.format :
                               shadow->maps.format,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .outputLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
            Graphics::RenderPassDesc renderPassDesc = {
                .depthAttachment = { attachment }
            };
            auto renderPass = device->CreateRenderPass(renderPassDesc);

            Graphics::FrameBufferDesc frameBufferDesc = {
                .renderPass = renderPass,
                .depthAttachment = { shadow->useCubemap ? shadow->cubemap.image : shadow->maps.image, 0, true},
                .extent = { uint32_t(shadow->resolution), uint32_t(shadow->resolution) }
            };
            return device->CreateFrameBuffer(frameBufferDesc);

        }

        PipelineConfig ShadowRenderer::GetPipelineConfigForSubData(Mesh::MeshSubData *subData,
            Mesh::Mesh* mesh, Ref<Graphics::FrameBuffer>& frameBuffer) {

            auto material = subData->material;

            auto shaderConfig = ShaderConfig {
                {"shadowMapping.vsh", VK_SHADER_STAGE_VERTEX_BIT},
                {"shadowMapping.fsh", VK_SHADER_STAGE_FRAGMENT_BIT},
            };
            auto pipelineDesc = Graphics::GraphicsPipelineDesc {
                .frameBuffer = frameBuffer,
                .vertexInputInfo = mesh->vertexArray.GetVertexInputState(),
            };

            if (material->twoSided || !mesh->cullBackFaces) {
                pipelineDesc.rasterizer.cullMode = VK_CULL_MODE_NONE;
            }

            std::vector<std::string> macros;
            if (material->HasOpacityMap()) {
                macros.push_back("OPACITY_MAP");
            }

            return PipelineConfig(shaderConfig, pipelineDesc, macros);

        }
    }

}