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

            std::swap(prevLightMap, lightMap);
            lightMap.clear();

            Ref<RenderList::Pass> shadowPass = renderList->PopPassFromQueue(RenderList::RenderPassType::Shadow);
            while (!renderList->doneProcessingShadows || shadowPass != nullptr) {
                if (!shadowPass) {
                    // We might need to wait for the next pass to be processed and culled
                    std::this_thread::yield();
                    shadowPass = renderList->PopPassFromQueue(RenderList::RenderPassType::Shadow);
                    continue;
                }

                ProcessPass(target, scene, commandList, renderList, shadowPass);

                shadowPass = renderList->PopPassFromQueue(RenderList::RenderPassType::Shadow);
            }

            // It might be that between asking for the last pass and checking doneProcessing, that 
            // another push to the queue has been happening. So check again here
            shadowPass = renderList->PopPassFromQueue(RenderList::RenderPassType::Shadow);
            if (shadowPass != nullptr) {
                ProcessPass(target, scene, commandList, renderList, shadowPass);
            }

            // Need to also keep track of non processed layer (e.g. long range layers)
            for (auto& [lightEntity, frameBuffer] : lightMap) {
                const auto& light = lightEntity.GetComponent<LightComponent>();
                // Will be activated automatically by movable lights
                light.shadow->update = false;
                
                if (light.type == LightType::DirectionalLight && light.shadow->longRange) {
                    // Need to go through render passes to make sure images have transitioned
                    frameBuffer->depthAttachment.layer = light.shadow->viewCount - 1;
                    frameBuffer->Refresh();
                    commandList->BeginRenderPass(frameBuffer->renderPass, frameBuffer, true);
                    commandList->EndRenderPass();
                }
            }

            Graphics::Profiler::EndQuery();

        }

        void ShadowRenderer::ProcessPass(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList, 
            RenderList* renderList, Ref<RenderList::Pass> shadowPass) {

            Graphics::Profiler::BeginQuery("Entity pass " + std::to_string(shadowPass->lightEntity) + " layer " + std::to_string(shadowPass->layer));

            auto lightEntity = shadowPass->lightEntity;
            auto& light = lightEntity.GetComponent<LightComponent>();
            if (!light.shadow || !light.shadow->update)
                return;

            auto sceneState = &scene->renderState;

            Ref<Graphics::FrameBuffer> frameBuffer = nullptr;
            if (lightMap.contains(lightEntity))
                frameBuffer = lightMap[lightEntity];
            else
                frameBuffer = GetOrCreateFrameBuffer(lightEntity);

            lightMap[lightEntity] = frameBuffer;

            if (frameBuffer->depthAttachment.layer != shadowPass->layer) {
                frameBuffer->depthAttachment.layer = shadowPass->layer;
                frameBuffer->Refresh();
            }

            auto shadow = light.shadow;

            bool isDirectionalLight = false;
            vec3 lightLocation;

            if (light.type == LightType::DirectionalLight) {
                lightLocation = 1000000.0f * -normalize(light.transformedProperties.directional.direction);
                isDirectionalLight = true;
            }
            else if (light.type == LightType::PointLight) {
                lightLocation = light.transformedProperties.point.position;
            }

            commandList->BindBuffer(shadowPass->currentMatricesBuffer, 1, 1);
            commandList->BindBuffer(shadowPass->impostorMatricesBuffer, 1, 3);

            commandList->BeginRenderPass(frameBuffer->renderPass, frameBuffer, true);

            auto component = &shadow->views[shadowPass->layer];
            auto lightSpaceMatrix = component->projectionMatrix * component->viewMatrix;

            // Bind wind map (need to bind for each render pass, since impostor renderer resets this binding)
            scene->wind.noiseMap.Bind(commandList, 3, 0);

            int32_t subDataCount = 0;
            // Retrieve all possible materials
            for (auto& [meshId, instances] : shadowPass->meshToInstancesMap) {
                if (!instances.count) continue;

                auto& mesh = renderList->meshIdToMeshMap[meshId];
                for (auto& subData : mesh->data.subData) {
                    if (!subData.material.IsLoaded())
                        continue;

                    if (subDataCount < subDatas.size())
                        subDatas[subDataCount] = { &subData, mesh.GetID(), mesh.Get().get() };
                    else
                        subDatas.push_back({ &subData, mesh.GetID(), mesh.Get().get() });
                    subDataCount++;
                }
            }

            // Check whether materials have pipeline configs
            for (int32_t i = 0; i < subDataCount; i++) {
                auto& [subData, _, mesh] = subDatas[i];
                const auto& material = subData->material;
                if (subData->shadowConfig.IsValid()) continue;

                subData->shadowConfig = GetPipelineConfigForSubData(subData, mesh, frameBuffer);
            }

            // Sort materials by hash
            std::sort(subDatas.begin(), subDatas.begin() + size_t(subDataCount),
                [](auto& subData0, auto& subData1) {
                    return std::get<0>(subData0)->shadowConfig.variantHash <
                        std::get<0>(subData1)->shadowConfig.variantHash;
                });

            Hash prevHash = 0;
            Hash prevMesh = 0;
            Ref<Graphics::Pipeline> currentPipeline = nullptr;
            for (int32_t i = 0; i < subDataCount; i++) {
                auto& [subData, meshID, mesh] = subDatas[i];
                const auto& instances = shadowPass->meshToInstancesMap[meshID];

                auto material = subData->material;

                // Overwrite material config frame buffer (and assume these have the same format)
                subData->shadowConfig.graphicsPipelineDesc.frameBuffer = frameBuffer;

                if (subData->shadowConfig.variantHash != prevHash) {
                    currentPipeline = PipelineManager::GetPipeline(subData->shadowConfig);
                    commandList->BindPipeline(currentPipeline);
                    prevHash = subData->shadowConfig.variantHash;
                }

                if (meshID != prevMesh) {
                    mesh->vertexArray.Bind(commandList);
                    prevMesh = meshID;
                }

#if !defined(AE_BINDLESS) || defined(AE_OS_MACOS)
                if (material->HasOpacityMap())
                    commandList->BindImage(material->opacityMap->image, material->opacityMap->sampler, 3, 1);
#endif

                auto pushConstants = PushConstants{
                    .lightSpaceMatrix = lightSpaceMatrix,
                    .vegetation = mesh->vegetation ? 1u : 0u,
                    .invertUVs = mesh->invertUVs ? 1u : 0u,
                    .windTextureLod = mesh->windNoiseTextureLod,
                    .windBendScale = mesh->windBendScale,
                    .windWiggleScale = mesh->windWiggleScale,
                    .textureID = material->HasOpacityMap() ? sceneState->textureToBindlessIdx[material->opacityMap.Get()] : 0
                };
                commandList->PushConstants("constants", &pushConstants);

                commandList->DrawIndexed(subData->indicesCount, instances.count, subData->indicesOffset,
                    0, instances.offset);

                // Reset the frame buffer here to let it be able to be deleted
                subData->shadowConfig.graphicsPipelineDesc.frameBuffer = nullptr;

            }

            impostorRenderer.Render(frameBuffer, commandList, renderList,
                shadowPass.get(), component->viewMatrix, component->projectionMatrix, lightLocation);

            commandList->EndRenderPass();

            Graphics::Profiler::EndQuery();

        }

        Ref<Graphics::FrameBuffer> ShadowRenderer::GetOrCreateFrameBuffer(Scene::Entity entity) {

            auto& light = entity.GetComponent<LightComponent>();
            auto& shadow = light.shadow;
            
            if (prevLightMap.contains(entity)) {
                auto frameBuffer = prevLightMap[entity];
                if (frameBuffer->extent.width == shadow->resolution ||
                    frameBuffer->extent.height == shadow->resolution) {
                    return frameBuffer;
                }
            }

            Graphics::RenderPassDepthAttachment attachment = {
                .imageFormat = shadow->useCubemap ? shadow->cubemap->format :
                               shadow->maps->format,
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
                .depthAttachment = { shadow->useCubemap ? shadow->cubemap->image : shadow->maps->image, 0, true},
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

            bool hasTexCoords = mesh->data.texCoords.ContainsData();

            std::vector<std::string> macros;
            if (material->HasOpacityMap() && hasTexCoords) {
                macros.push_back("OPACITY_MAP");
            }

#if defined(AE_BINDLESS) && !defined(AE_OS_MACOS)
            macros.push_back("BINDLESS_TEXTURES");
#endif

            return PipelineConfig(shaderConfig, pipelineDesc, macros);

        }
    }

}