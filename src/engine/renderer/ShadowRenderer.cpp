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
            commandList->BindBuffer(shadowPass->currentJointMatricesBuffer, 1, 4);
            if (shadowPass->softBodyVertexOffsetsBuffer) {
                commandList->BindBuffer(renderList->currentSoftBodyVerticesBuffer, 1, 24);
                commandList->BindBuffer(shadowPass->softBodyVertexOffsetsBuffer, 1, 26);
            }

            // Passes culled on the host have their own instance indices
            auto& gpuScene = scene->renderState.gpuScene;
            if (shadowPass->gpu.culled) {
                commandList->BindBuffer(gpuScene.indexBuffer, 1, 22);
                commandList->BindBuffer(gpuScene.indexBuffer, 1, 23);
            }
            else {
                commandList->BindBuffer(shadowPass->instanceIndicesBuffer, 1, 22);
                commandList->BindBuffer(shadowPass->impostorInstanceIndicesBuffer, 1, 23);
            }

            auto component = &shadow->views[shadowPass->layer];
            auto lightSpaceMatrix = component->projectionMatrix * component->viewMatrix;

            auto createConfig = [&](Mesh::MeshSubData* subData, Mesh::Mesh* mesh, const MaterialGraph::CompiledGraph* graph) {
                return GetPipelineConfigForSubData(subData, mesh, attachmentFormats, graph);
            };
            // Graphs which neither move vertices nor cut out parts have the shadows of the regular pipeline
            auto hasGraphPipeline = [](const MaterialGraph::CompiledGraph& graph) { return graph.UsesShadowPipeline(); };
            // Pipelines which can discard fragments last, so the others keep the early depth test
            auto getOrder = [](PipelineConfig& config, bool usesGraph) {
                return usesGraph || config.HasMacro("OPACITY_MAP") || config.HasMacro("TRANSLUCENT") ? 1u : 0u;
            };
            drawer.Prepare(renderList, *shadowPass, shadowPass->gpu.draws, true, &Mesh::MeshSubData::shadowPipelines,
                createConfig, hasGraphPipeline, getOrder);

            // Shadow maps drawn with defaults or with casters which change every frame aren't kept
            auto isFinal = true;
            auto checkFinal = [&](const Helper::SubDataDrawer::Entry& entry) {
                if (!isFinal)
                    return;

                const auto& pipelines = entry.subData->shadowPipelines;
                const auto& material = entry.subData->material;
                auto readsTexCoords = pipelines.usesGraph ? pipelines.graphReadsTexCoords : material->HasOpacityMap();
                isFinal = !pipelines.graphPending && !pipelines.graphAnimatesShadow &&
                    !(readsTexCoords && material->uvAnimation != vec2(0.0f));
                if (isFinal && pipelines.usesGraph) {
                    auto graphData = sceneState->materialGraphMap.find(material.Get().get());
                    isFinal = graphData != sceneState->materialGraphMap.end() && !graphData->second.hasDefaultTextures;
                }
                else if (isFinal && material->HasOpacityMap()) {
                    auto materialIt = sceneState->materialMap.find(material.Get().get());
                    isFinal = materialIt != sceneState->materialMap.end() && materialIt->second < sceneState->materialTextureIndices.size() &&
                        !sceneState->IsDefaultTextureSlot(sceneState->materialTextureIndices[materialIt->second].opacity);
                }
            };

            // Draws of pipelines with draw data can be merged, only their wind and opacity differ
            auto fillDrawData = [&](const Helper::SubDataDrawer::Entry& entry) {
                checkFinal(entry);
                const auto& mesh = entry.mesh;
                return DrawData {
                    .windTextureLod = mesh->windNoiseTextureLod,
                    .windBendScale = mesh->windBendScale,
                    .windWiggleScale = mesh->windWiggleScale,
                    .opacity = entry.subData->material->opacity
                };
            };

            PushConstants pushConstants;
            auto fillPushConstants = [&](const Helper::SubDataDrawer::Entry& entry, const RenderList::MeshInstances& instances,
                bool usesGraph) -> PushConstants& {
                const auto& mesh = entry.mesh;
                const auto& material = entry.subData->material;

                auto materialIt = sceneState->materialMap.find(material.Get().get());
                auto materialIdx = materialIt != sceneState->materialMap.end() ? uint32_t(materialIt->second) : 0u;
                auto opacityTextureIdx = materialIdx < sceneState->materialTextureIndices.size() ?
                    sceneState->materialTextureIndices[materialIdx].opacity : 0u;

                pushConstants = PushConstants{
                    .lightSpaceMatrix = lightSpaceMatrix,
                    .deformOffset = uint32_t(instances.deformOffset),
                    .jointCount = uint32_t(mesh->data.skeleton.GetJointCount()),
                    .instanceOffset = uint32_t(instances.offset),
                    .invertUVs = mesh->invertUVs ? 1u : 0u,
                    .textureID = opacityTextureIdx,
                    .uvAnimationX = material->uvAnimation.x,
                    .uvAnimationY = material->uvAnimation.y,
                    .uvTiling = material->tiling,
                    .draw = fillDrawData(entry)
                };

                if (usesGraph) {
                    auto graphData = sceneState->materialGraphMap.find(material.Get().get());
                    if (graphData != sceneState->materialGraphMap.end())
                        pushConstants.materialGraphOffset = graphData->second.parameterOffset;
                }
                return pushConstants;
            };

            drawer.PrepareDrawData(commandList, &Mesh::MeshSubData::shadowPipelines, fillDrawData);

            commandList->BeginRendering(rendering);

            // Bind wind map (need to bind for each render pass, since impostor renderer resets this binding)
            scene->renderState.wind.noiseMap.Bind(commandList, 3, 0);

            if (!scene->renderState.materialGraphParameters.empty())
                scene->renderState.materialGraphBuffer.Bind(commandList, 3, 9);

            // Shadows don't color the levels of detail
            auto setLod = [](uint32_t) { return false; };

            auto drawnAll = drawer.Draw(commandList, renderList, *shadowPass, &Mesh::MeshSubData::shadowPipelines,
                fillPushConstants, setLod, 10);

            profilerScope.Next("Shadow impostors");
            impostorRenderer.Render(attachmentFormats, commandList, renderList,
                shadowPass.get(), component->viewMatrix, component->projectionMatrix, lightLocation);

            commandList->EndRendering();

            shadowPass->keepsShadowMap = isFinal && drawnAll;
            shadowPass->drawnContentHash = shadowPass->keepsShadowMap ? shadowPass->contentHash : 0;

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

        PipelineConfig ShadowRenderer::GetPipelineConfigForSubData(Mesh::MeshSubData *subData,
            Mesh::Mesh* mesh, const Graphics::AttachmentFormats& attachmentFormats, const MaterialGraph::CompiledGraph* graph) {

            auto material = subData->material;

            bool hasTexCoords = mesh->data.texCoords.ContainsData();
            bool hasOpacityMap = material->HasOpacityMap() && hasTexCoords && (!graph || !graph->hasSurface);
            bool hasGraphOpacity = graph && graph->CutsOutFragments();
            // Translucent surfaces let light through by their opacity, see shadowMapping.fsh
            bool translucent = material->IsTranslucent();

            // Without an opacity only depth is written, which doesn't need a fragment shader
            auto shaderConfig = ShaderConfig {
                {graph ? graph->shadowVertexFile : "shadowMapping.vsh", VK_SHADER_STAGE_VERTEX_BIT},
            };
            if (hasOpacityMap || hasGraphOpacity || translucent) {
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
            if (mesh->vegetation) {
                macros.push_back("VEGETATION");
            }
            // Depth only draws of the GPU culling are merged into multi draws, their data comes from a buffer
            if (!graph && !hasOpacityMap && !mesh->data.IsSkinned()) {
                macros.push_back("DRAW_DATA");
            }
            if (hasOpacityMap) {
                macros.push_back("OPACITY_MAP");
            }
            if (translucent) {
                macros.push_back("TRANSLUCENT");
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
            if (graph && mesh->data.texCoords1.ContainsData()) {
                macros.push_back("TEX_COORDS_1");
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
