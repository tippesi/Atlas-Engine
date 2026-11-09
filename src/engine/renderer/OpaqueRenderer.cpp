#include "OpaqueRenderer.h"

#include "../Clock.h"
#include "../materialgraph/PipelineCache.h"

#include <mutex>
#include <tuple>

namespace Atlas {

    namespace Renderer {

        void OpaqueRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

        }

        void OpaqueRenderer::Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList, 
            RenderList* renderList, std::unordered_map<void*, uint16_t>& materialMap, bool lateDraws) {

            CPUProfiler::Scope profilerScope("Opaque geometry");
            GPUProfiler::Scope gpuScope("Opaque geometry");

            auto mainPass = lateDraws ? renderList->GetMainPass() : renderList->PopPassFromQueue(RenderList::RenderPassType::Main);
            if (!mainPass)
                return;

            auto sceneState = &scene->renderState;

            commandList->BindBuffer(mainPass->currentMatricesBuffer, 1, 1);
            commandList->BindBuffer(mainPass->lastMatricesBuffer, 1, 2);
            commandList->BindBuffer(mainPass->impostorMatricesBuffer, 1, 3);
            commandList->BindBuffer(mainPass->currentJointMatricesBuffer, 1, 4);
            commandList->BindBuffer(mainPass->lastJointMatricesBuffer, 1, 5);

            // Bind wind map
            scene->renderState.wind.noiseMap.Bind(commandList, 3, 8);

            if (!sceneState->materialGraphParameters.empty())
                sceneState->materialGraphBuffer.Bind(commandList, 3, 9);

            int32_t subDataCount = 0;
            // Retrieve all possible materials;
            for (const auto& [meshId, instances] : mainPass->meshToInstancesMap) {
                if (!instances.count || lateDraws) continue;

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

            for (const auto& draw : lateDraws ? mainPass->gpuLateDraws : mainPass->gpuDraws) {
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
                if (subData->mainGraphPending)
                    UpdateGraphPipelines(subData, mesh, target);
                if (subData->mainConfig.IsValid()) continue;

                subData->mainConfig = GetPipelineConfigForSubData(subData, mesh, target);
                subData->mainReversedWindingConfig = subData->mainConfig.GetReversedWindingVariant();

                if (material->HasGraph()) {
                    subData->mainGraphPending = true;
                    UpdateGraphPipelines(subData, mesh, target);
                }
            }

            auto getConfig = [](const auto& entry) -> PipelineConfig& {
                const auto& [subData, meshID, mesh, gpuDraw] = entry;
                return gpuDraw && gpuDraw->type == RenderList::GPUDrawType::ReversedWinding ?
                    subData->mainReversedWindingConfig : subData->mainConfig;
            };

            // Sort materials by hash
            std::sort(subDatas.begin(), subDatas.begin() + size_t(subDataCount),
                [&](auto& subData0, auto& subData1) {
                    return getConfig(subData0).variantHash < getConfig(subData1).variantHash;
                });

            Hash prevMesh = 0;
            Hash prevHash = 0;
            Ref<Graphics::Pipeline> currentPipeline;
            const Graphics::PushConstantRange* constantsRange = nullptr;
            for (int32_t i = 0; i < subDataCount; i++) {
                auto& [subData, meshID, mesh, gpuDraw] = subDatas[i];
                auto instances = gpuDraw ? RenderList::MeshInstances{} : mainPass->meshToInstancesMap[meshID];

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

                auto materialIt = materialMap.find(material.Get().get());
                auto materialIdx = materialIt != materialMap.end() ? uint32_t(materialIt->second) : 0u;
                auto textureIndices = materialIdx < sceneState->materialTextureIndices.size() ?
                    sceneState->materialTextureIndices[materialIdx] : Scene::MaterialTextureIndices();

                auto pushConstants = PushConstants {
                    .vegetation = mesh->vegetation ? 1u : 0u,
                    .invertUVs = mesh->invertUVs ? 1u : 0u,
                    .twoSided = material->twoSided ? 1u : 0u,
                    .staticMesh = mesh->mobility == Mesh::MeshMobility::Stationary ? 1u : 0u,
                    .materialIdx = materialIdx,
                    .normalScale = material->normalScale,
                    .displacementScale = material->displacementScale,
                    .windTextureLod = mesh->windNoiseTextureLod,
                    .windBendScale = mesh->windBendScale,
                    .windWiggleScale = mesh->windWiggleScale,
                    .uvAnimationX = material->uvAnimation.x,
                    .uvAnimationY = material->uvAnimation.y,
                    .uvTiling = material->tiling,
                    .baseColorTextureIdx = textureIndices.baseColor,
                    .opacityTextureIdx = textureIndices.opacity,
                    .normalTextureIdx = textureIndices.normal,
                    .roughnessTextureIdx = textureIndices.roughness,
                    .metalnessTextureIdx = textureIndices.metalness,
                    .aoTextureIdx = textureIndices.ao,
                    .heightTextureIdx = textureIndices.height,
                    .emissiveTextureIdx = textureIndices.emissive,
                    .jointOffset = uint32_t(instances.jointOffset),
                    .jointCount = uint32_t(mesh->data.skeleton.GetJointCount()),
                    .instanceOffset = uint32_t(instances.offset),
                };

                if (subData->mainUsesGraph) {
                    auto graphData = sceneState->materialGraphMap.find(material.Get().get());
                    if (graphData != sceneState->materialGraphMap.end()) {
                        pushConstants.materialIdx = graphData->second.materialIdx;
                        pushConstants.materialGraphOffset = graphData->second.parameterOffset;
                    }
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
                    auto& reversedWindingConfig = subData->mainReversedWindingConfig;
                    currentPipeline = PipelineManager::GetPipeline(reversedWindingConfig);
                    commandList->BindPipeline(currentPipeline);
                    constantsRange = currentPipeline->shader->GetPushConstantRange("constants");
                    commandList->PushConstants(constantsRange, &pushConstants);
                    commandList->DrawIndexed(subData->indicesCount, instances.reversedWindingCount, subData->indicesOffset,
                        0, instances.reversedWindingOffset);
                    prevHash = reversedWindingConfig.variantHash;
                }

            }

        }

        void OpaqueRenderer::UpdateGraphPipelines(Mesh::MeshSubData* subData, Mesh::Mesh* mesh, const Ref<RenderTarget>& target) {

            using PipelineState = MaterialGraph::PipelineCache::State;

            const auto& graph = subData->material->graph;
            auto compiled = graph.IsLoaded() ? graph->GetCompiled() : nullptr;
            if (!compiled || !compiled->valid || compiled->mainVertexFile.empty()) {
                subData->mainGraphPending = false;
                return;
            }

            auto config = GetPipelineConfigForSubData(subData, mesh, target, compiled.get());
            auto reversedWindingConfig = config.GetReversedWindingVariant();

            auto blocking = graph->blockingPipelineUsers > 0;
            auto state = MaterialGraph::PipelineCache::Request(config, blocking);
            auto reversedWindingState = MaterialGraph::PipelineCache::Request(reversedWindingConfig, blocking);
            if (state == PipelineState::Pending || reversedWindingState == PipelineState::Pending)
                return;

            subData->mainGraphPending = false;
            if (state == PipelineState::Failed || reversedWindingState == PipelineState::Failed)
                return;

            subData->mainConfig = config;
            subData->mainReversedWindingConfig = reversedWindingConfig;

            subData->mainUsesGraph = true;

        }

        PipelineConfig OpaqueRenderer::GetPipelineConfigForSubData(Mesh::MeshSubData *subData,
            Mesh::Mesh* mesh, const Ref<RenderTarget>& target, const MaterialGraph::CompiledGraph* graph) {

            auto shaderConfig = ShaderConfig {
                {graph ? graph->mainVertexFile : "deferred/geometry.vsh", VK_SHADER_STAGE_VERTEX_BIT},
                {graph ? graph->mainFragmentFile : "deferred/geometry.fsh", VK_SHADER_STAGE_FRAGMENT_BIT},
            };
            auto pipelineDesc = Graphics::GraphicsPipelineDesc{
                .attachmentFormats = Graphics::AttachmentFormats(target->gBufferRendering),
                .vertexInputInfo = mesh->vertexArray.GetVertexInputState(),
            };

            auto material = subData->material;

            if (material->twoSided || !mesh->cullBackFaces) {
                pipelineDesc.rasterizer.cullMode = VK_CULL_MODE_NONE;
            }

            bool hasTangents = mesh->data.tangents.ContainsData();
            bool hasTexCoords = mesh->data.texCoords.ContainsData();

            // The surface of a graph replaces all the maps
            auto usesMaps = !graph || !graph->hasSurface;

            std::vector<std::string> macros;
            // Pipelines are rebound when the variant hash changes, which doesn't know about shaders
            // Regular materials are sorted by it, so it has to stay as it is for them
            if (graph) {
                macros.push_back("MATERIAL_GRAPH_" + std::to_string(graph->hash));
            }
            if (usesMaps && material->HasBaseColorMap() && hasTexCoords) {
                macros.push_back("BASE_COLOR_MAP");
            }
            if (usesMaps && material->HasOpacityMap() && hasTexCoords) {
                macros.push_back("OPACITY_MAP");
            }
            if (usesMaps && material->HasNormalMap() && hasTangents && hasTexCoords) {
                macros.push_back("NORMAL_MAP");
            }
            if (usesMaps && material->HasRoughnessMap() && hasTexCoords) {
                macros.push_back("ROUGHNESS_MAP");
            }
            if (usesMaps && material->HasMetalnessMap() && hasTexCoords) {
                macros.push_back("METALNESS_MAP");
            }
            if (usesMaps && material->HasAoMap() && hasTexCoords) {
                macros.push_back("AO_MAP");
            }
            if (usesMaps && material->HasDisplacementMap() && hasTangents && hasTexCoords) {
                macros.push_back("HEIGHT_MAP");
            }
            if (usesMaps && material->HasEmissiveMap() && hasTexCoords) {
                macros.push_back("EMISSIVE_MAP");
            }
            // This is a check if we have any maps at all (no macros, no maps)
            if (hasTexCoords) {
                macros.push_back("TEX_COORDS");
            }
            if (hasTangents) {
                macros.push_back("TANGENTS");
            }
            if (usesMaps && glm::length(material->emissiveColor) > 0.0f) {
                macros.push_back("EMISSIVE");
            }
            if (mesh->data.colors.ContainsData()) {
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
