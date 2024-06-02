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

            lightMap.clear();

            Ref<RenderList::Pass> shadowPass = renderList->PopPassFromQueue(RenderList::RenderPassType::Shadow);
            while (!renderList->doneProcessingShadows || shadowPass != nullptr) {
                if (!shadowPass) {
                    // We might need to wait for the next pass to be processed and culled
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                    shadowPass = renderList->PopPassFromQueue(RenderList::RenderPassType::Shadow);
                    continue;
                }

                ProcessPass(target, scene, commandList, shadowPass);

                shadowPass = renderList->PopPassFromQueue(RenderList::RenderPassType::Shadow);
            }

            // It might be that between asking for the last pass and checking doneProcessing, that 
            // another push to the queue has been happening. So check again here
            shadowPass = renderList->PopPassFromQueue(RenderList::RenderPassType::Shadow);
            if (shadowPass != nullptr) {
                ProcessPass(target, scene, commandList, shadowPass);
            }

            // Need to also keep track of non processed layer (e.g. long range layers)
            for (auto& [lightEntity, frameBuffer] : lightMap) {
                auto& light = lightEntity.GetComponent<LightComponent>();
                
                if (light.type == LightType::DirectionalLight && light.shadow->longRange) {
                    // Need to go through render passes to make sure images have transitioned
                    commandList->BeginRenderPass(frameBuffer->renderPass, frameBuffer, true);
                    commandList->EndRenderPass();
                }
            }

            Graphics::Profiler::EndQuery();

        }

        void ShadowRenderer::ProcessPass(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList, Ref<RenderList::Pass> shadowPass) {

            auto lightEntity = shadowPass->lightEntity;
            auto& light = lightEntity.GetComponent<LightComponent>();
            if (!light.shadow || !light.shadow->update)
                return;

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
            scene->wind.noiseMap.Bind(commandList, 3, 1);

            int32_t subDataCount = 0;
            // Retrieve all possible materials
            for (auto& [meshId, instances] : shadowPass->meshToInstancesMap) {
                if (!instances.count) continue;

                auto& mesh = shadowPass->meshIdToMeshMap[meshId];
                for (auto& subData : mesh->data.subData) {
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

                auto pushConstants = PushConstants{
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

            impostorRenderer.Render(frameBuffer, commandList,
                shadowPass.get(), component->viewMatrix, component->projectionMatrix, lightLocation);

            commandList->EndRenderPass();

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