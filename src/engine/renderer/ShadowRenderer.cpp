#include "ShadowRenderer.h"
#include "../materialgraph/PipelineCache.h"

#include "../Clock.h"

namespace Atlas {

    namespace Renderer {

        void ShadowRenderer::Init(Graphics::GraphicsDevice* device) {

            this->device = device;

            impostorRenderer.Init(device);

        }

        void ShadowRenderer::Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList, RenderList* renderList) {

            CPUProfiler::Scope profilerScope("Shadows");
            GPUProfiler::Scope gpuScope("Shadows");

            processedLights.clear();

            Ref<RenderList::Pass> shadowPass = renderList->PopPassFromQueue(RenderList::RenderPassType::Shadow);
            while (!scene->renderState.fillShadowRenderPassesJob.HasFinished() || shadowPass != nullptr) {
                if (!shadowPass) {
                    // We might need to wait for the next pass to be processed and culled
                    CPUProfiler::Scope waitScope("Wait for shadow passes");
                    while (!shadowPass && !scene->renderState.fillShadowRenderPassesJob.HasFinished()) {
                        std::this_thread::yield();
                        shadowPass = renderList->PopPassFromQueue(RenderList::RenderPassType::Shadow);
                    }
                    continue;
                }

                ProcessPass(target, scene, commandList, renderList, shadowPass);

                shadowPass = renderList->PopPassFromQueue(RenderList::RenderPassType::Shadow);
            }

            // Several workers might have pushed to the queue between asking for the last pass and checking
            // that the job finished, so empty it now
            shadowPass = renderList->PopPassFromQueue(RenderList::RenderPassType::Shadow);
            while (shadowPass != nullptr) {
                ProcessPass(target, scene, commandList, renderList, shadowPass);
                shadowPass = renderList->PopPassFromQueue(RenderList::RenderPassType::Shadow);
            }

            // Need to also keep track of non processed layer (e.g. long range layers)
            for (auto lightEntity : processedLights) {
                auto light = scene->renderState.GetLight(lightEntity);
                
                if (light && light->comp.type == LightType::DirectionalLight && light->comp.shadow->longRange) {
                    // Need to go through render passes to make sure images have transitioned
                    commandList->BeginRendering(GetRendering(light->comp, light->comp.shadow->viewCount - 1));
                    commandList->EndRendering();
                }
            }

        }

        void ShadowRenderer::ProcessPass(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList,
            RenderList* renderList, Ref<RenderList::Pass> shadowPass) {

            CPUProfiler::Scope profilerScope("Shadow pass");
            GPUProfiler::Scope gpuScope("Entity pass " + std::to_string(shadowPass->lightEntity) + " layer " + std::to_string(shadowPass->layer));

            auto lightEntity = shadowPass->lightEntity;
            auto extractedLight = scene->renderState.GetLight(lightEntity);
            if (!extractedLight || !extractedLight->updateShadow)
                return;

            auto& light = extractedLight->comp;

            auto sceneState = &scene->renderState;
            auto rendering = GetRendering(light, shadowPass->layer);
            auto attachmentFormats = Graphics::AttachmentFormats(rendering);

            processedLights.insert(lightEntity);

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
            commandList->BindBuffer(shadowPass->currentJointMatricesBuffer, 1, 4);

            // Passes culled on the host have their own instance indices
            auto& gpuScene = scene->renderState.gpuScene;
            if (shadowPass->gpuCulled) {
                commandList->BindBuffer(gpuScene.indexBuffer, 1, 22);
                commandList->BindBuffer(gpuScene.impostorIndexBuffer, 1, 23);
            }
            else {
                commandList->BindBuffer(shadowPass->instanceIndicesBuffer, 1, 22);
                commandList->BindBuffer(shadowPass->impostorInstanceIndicesBuffer, 1, 23);
            }

            commandList->BeginRendering(rendering);

            auto component = &shadow->views[shadowPass->layer];
            auto lightSpaceMatrix = component->projectionMatrix * component->viewMatrix;

            // Bind wind map (need to bind for each render pass, since impostor renderer resets this binding)
            scene->renderState.wind.noiseMap.Bind(commandList, 3, 0);

            if (!scene->renderState.materialGraphParameters.empty())
                scene->renderState.materialGraphBuffer.Bind(commandList, 3, 9);

            int32_t subDataCount = 0;
            // Retrieve all possible materials
            for (auto& [meshId, instances] : shadowPass->meshToInstancesMap) {
                if (!instances.count) continue;

                auto& mesh = renderList->meshIdToMeshMap[meshId];
                for (auto& subData : mesh->data.subData) {
                    if (!subData.material.IsLoaded())
                        continue;

                    if (subDataCount < subDatas.size())
                        subDatas[subDataCount] = { &subData, mesh.GetID(), mesh.Get().get(), nullptr };
                    else
                        subDatas.push_back({ &subData, mesh.GetID(), mesh.Get().get(), nullptr });
                    subDataCount++;
                }
            }

            for (const auto& draw : shadowPass->gpuDraws) {
                if (!draw.subData)
                    continue;

                if (subDataCount < subDatas.size())
                    subDatas[subDataCount] = { draw.subData, draw.meshId, draw.mesh, &draw };
                else
                    subDatas.push_back({ draw.subData, draw.meshId, draw.mesh, &draw });
                subDataCount++;
            }

            // Check whether materials have pipeline configs
            for (int32_t i = 0; i < subDataCount; i++) {
                auto& [subData, _, mesh, gpuDraw] = subDatas[i];
                const auto& material = subData->material;
                if (subData->shadowGraphPending)
                    UpdateGraphPipelines(subData, mesh, attachmentFormats);
                if (subData->shadowConfig.IsValid()) continue;

                subData->shadowConfig = GetPipelineConfigForSubData(subData, mesh, attachmentFormats);
                subData->shadowReversedWindingConfig = subData->shadowConfig.GetReversedWindingVariant();

                if (material->HasGraph()) {
                    subData->shadowGraphPending = true;
                    UpdateGraphPipelines(subData, mesh, attachmentFormats);
                }
            }

            auto getConfig = [](const auto& entry) -> PipelineConfig& {
                const auto& [subData, meshID, mesh, gpuDraw] = entry;
                return gpuDraw && gpuDraw->type == RenderList::GPUDrawType::ReversedWinding ?
                    subData->shadowReversedWindingConfig : subData->shadowConfig;
            };

            // Sort materials by hash
            std::sort(subDatas.begin(), subDatas.begin() + size_t(subDataCount),
                [&](auto& subData0, auto& subData1) {
                    return getConfig(subData0).variantHash < getConfig(subData1).variantHash;
                });

            Hash prevHash = 0;
            Hash prevMesh = 0;
            Ref<Graphics::Pipeline> currentPipeline = nullptr;
            const Graphics::PushConstantRange* constantsRange = nullptr;
            for (int32_t i = 0; i < subDataCount; i++) {
                auto& [subData, meshID, mesh, gpuDraw] = subDatas[i];
                auto instances = gpuDraw ? RenderList::MeshInstances{} : shadowPass->meshToInstancesMap[meshID];

                const auto& material = subData->material;

                auto& config = getConfig(subDatas[i]);

                if (config.variantHash != prevHash) {
                    currentPipeline = PipelineManager::GetPipeline(config);
                    commandList->BindPipeline(currentPipeline);
                    constantsRange = currentPipeline->shader->GetPushConstantRange("constants");
                    prevHash = config.variantHash;
                }

                if (meshID != prevMesh) {
                    mesh->vertexArray.Bind(commandList);
                    prevMesh = meshID;
                }

                auto materialIt = sceneState->materialMap.find(material.Get().get());
                auto materialIdx = materialIt != sceneState->materialMap.end() ? uint32_t(materialIt->second) : 0u;
                auto opacityTextureIdx = materialIdx < sceneState->materialTextureIndices.size() ?
                    sceneState->materialTextureIndices[materialIdx].opacity : 0u;

                auto pushConstants = PushConstants{
                    .lightSpaceMatrix = lightSpaceMatrix,
                    .vegetation = mesh->vegetation ? 1u : 0u,
                    .invertUVs = mesh->invertUVs ? 1u : 0u,
                    .windTextureLod = mesh->windNoiseTextureLod,
                    .windBendScale = mesh->windBendScale,
                    .windWiggleScale = mesh->windWiggleScale,
                    .textureID = opacityTextureIdx,
                    .jointOffset = uint32_t(instances.jointOffset),
                    .jointCount = uint32_t(mesh->data.skeleton.GetJointCount()),
                    .instanceOffset = uint32_t(instances.offset),
                };

                if (subData->shadowUsesGraph) {
                    auto graphData = sceneState->materialGraphMap.find(material.Get().get());
                    if (graphData != sceneState->materialGraphMap.end())
                        pushConstants.materialGraphOffset = graphData->second.parameterOffset;
                }

                commandList->PushConstants(constantsRange, &pushConstants);

                if (gpuDraw) {
                    renderList->DrawGPUCulled(commandList, *gpuDraw);
                    continue;
                }

                const auto defaultWindingCount = instances.count - instances.reversedWindingCount;
                if (defaultWindingCount) {
                    commandList->DrawIndexed(subData->indicesCount, defaultWindingCount, subData->indicesOffset,
                        0, instances.offset);
                }
                if (instances.reversedWindingCount) {
                    auto& reversedWindingConfig = subData->shadowReversedWindingConfig;
                    currentPipeline = PipelineManager::GetPipeline(reversedWindingConfig);
                    commandList->BindPipeline(currentPipeline);
                    constantsRange = currentPipeline->shader->GetPushConstantRange("constants");
                    commandList->PushConstants(constantsRange, &pushConstants);
                    commandList->DrawIndexed(subData->indicesCount, instances.reversedWindingCount, subData->indicesOffset,
                        0, instances.reversedWindingOffset);
                    prevHash = reversedWindingConfig.variantHash;
                }

            }

            profilerScope.Next("Shadow impostors");
            impostorRenderer.Render(attachmentFormats, commandList, renderList,
                shadowPass.get(), component->viewMatrix, component->projectionMatrix, lightLocation);

            commandList->EndRendering();

        }

        Graphics::RenderingDesc ShadowRenderer::GetRendering(const LightComponent& light, uint32_t layer) {

            const auto& shadow = light.shadow;
            return Graphics::RenderingDesc{
                .depthAttachment = {
                    .image = shadow->useCubemap ? shadow->cubemap->image : shadow->maps->image,
                    .layer = layer,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR
                }
            };

        }

        void ShadowRenderer::UpdateGraphPipelines(Mesh::MeshSubData* subData, Mesh::Mesh* mesh,
            const Graphics::AttachmentFormats& attachmentFormats) {

            using PipelineState = MaterialGraph::PipelineCache::State;

            const auto& graph = subData->material->graph;
            auto compiled = graph.IsLoaded() ? graph->GetCompiled() : nullptr;
            // Graphs which neither move vertices nor cut out parts have the shadows of the regular pipeline
            if (!compiled || !compiled->valid || !compiled->UsesShadowPipeline()) {
                subData->shadowGraphPending = false;
                return;
            }

            auto config = GetPipelineConfigForSubData(subData, mesh, attachmentFormats, compiled.get());
            auto reversedWindingConfig = config.GetReversedWindingVariant();

            auto blocking = graph->blockingPipelineUsers > 0;
            auto state = MaterialGraph::PipelineCache::Request(config, blocking);
            auto reversedWindingState = MaterialGraph::PipelineCache::Request(reversedWindingConfig, blocking);
            if (state == PipelineState::Pending || reversedWindingState == PipelineState::Pending)
                return;

            subData->shadowGraphPending = false;
            if (state == PipelineState::Failed || reversedWindingState == PipelineState::Failed)
                return;

            subData->shadowConfig = config;
            subData->shadowReversedWindingConfig = reversedWindingConfig;

            subData->shadowUsesGraph = true;

        }

        PipelineConfig ShadowRenderer::GetPipelineConfigForSubData(Mesh::MeshSubData *subData,
            Mesh::Mesh* mesh, const Graphics::AttachmentFormats& attachmentFormats, const MaterialGraph::CompiledGraph* graph) {

            auto material = subData->material;

            bool hasTexCoords = mesh->data.texCoords.ContainsData();
            bool hasOpacityMap = material->HasOpacityMap() && hasTexCoords && (!graph || !graph->hasSurface);
            bool hasGraphOpacity = graph && graph->CutsOutFragments();

            // Without an opacity map only depth is written, which doesn't need a fragment shader
            auto shaderConfig = ShaderConfig {
                {graph ? graph->shadowVertexFile : "shadowMapping.vsh", VK_SHADER_STAGE_VERTEX_BIT},
            };
            if (hasOpacityMap || hasGraphOpacity) {
                shaderConfig.push_back({graph ? graph->shadowFragmentFile : "shadowMapping.fsh", VK_SHADER_STAGE_FRAGMENT_BIT});
            }

            auto pipelineDesc = Graphics::GraphicsPipelineDesc {
                .attachmentFormats = attachmentFormats,
                .vertexInputInfo = mesh->vertexArray.GetVertexInputState(),
            };

            if (material->twoSided || !mesh->cullBackFaces) {
                pipelineDesc.rasterizer.cullMode = VK_CULL_MODE_NONE;
            }

            std::vector<std::string> macros;
            // Pipelines are rebound when the variant hash changes, which doesn't know about shaders
            if (graph) {
                macros.push_back("MATERIAL_GRAPH_" + std::to_string(graph->hash));
            }
            if (hasOpacityMap) {
                macros.push_back("OPACITY_MAP");
            }
            if (graph && hasTexCoords) {
                macros.push_back("TEX_COORDS");
            }
            if (graph && mesh->data.tangents.ContainsData()) {
                macros.push_back("TANGENTS");
            }
            if (graph && mesh->data.colors.ContainsData()) {
                macros.push_back("VERTEX_COLORS");
            }
            if (mesh->data.IsSkinned()) {
                macros.push_back("SKINNED");
            }

            if (!mesh->data.IsSkinned()) {
                macros.push_back("INSTANCE_BUFFER");
            }

            return PipelineConfig(shaderConfig, pipelineDesc, macros);

        }
    }

}
