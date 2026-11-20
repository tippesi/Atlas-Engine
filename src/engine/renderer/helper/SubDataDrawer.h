#pragma once

#include "../../System.h"
#include "RenderList.h"
#include "../../pipeline/PipelineManager.h"
#include "../../materialgraph/PipelineCache.h"

#include <algorithm>
#include <span>

namespace Atlas {

    namespace Renderer {

        namespace Helper {

            // Draws the sub data of a pass with their materials sorted by pipeline, the passes differ in their pipelines
            // (members of the sub data) and push constants
            class SubDataDrawer {

            public:
                struct Entry {
                    Mesh::MeshSubData* subData;
                    Hash meshId;
                    Mesh::Mesh* mesh;
                    // Draws culled on the GPU have a draw command, simulated soft bodies are drawn apart from the other instances
                    const RenderList::GPUDraw* gpuDraw;
                    bool softBody;
                };

                // Collects the host culled instances (if hostInstances) and the GPU draws, creates the missing pipelines
                // and sorts them. createConfig(subData, mesh, graph) returns the config of the pass, without a graph the
                // regular one. A graph only has a pipeline in the pass if hasGraphPipeline(graph) returns true
                template<class CreateConfig, class HasGraphPipeline>
                void Prepare(RenderList* renderList, const RenderList::Pass& pass, std::span<const RenderList::GPUDraw> gpuDraws,
                    bool hostInstances, Mesh::MeshSubDataPipelines Mesh::MeshSubData::* pipelines, const CreateConfig& createConfig,
                    const HasGraphPipeline& hasGraphPipeline) {

                    count = 0;
                    auto add = [&](const Entry& entry) {
                        if (count < entries.size())
                            entries[count] = entry;
                        else
                            entries.push_back(entry);
                        count++;
                    };

                    auto addInstances = [&](const std::unordered_map<size_t, RenderList::MeshInstances>& meshToInstances, bool softBody) {
                        for (const auto& [meshId, instances] : meshToInstances) {
                            if (!instances.count)
                                continue;

                            auto& mesh = renderList->meshIdToMeshMap[meshId];
                            for (auto& subData : mesh->data.subData) {
                                if (subData.material.IsLoaded())
                                    add({ &subData, mesh.GetID(), mesh.Get().get(), nullptr, softBody });
                            }
                        }
                    };
                    if (hostInstances) {
                        addInstances(pass.meshToInstancesMap, false);
                        addInstances(pass.meshToSoftBodyInstancesMap, true);
                    }

                    for (const auto& draw : gpuDraws) {
                        if (draw.subData)
                            add({ draw.subData, draw.meshId, draw.mesh, &draw, false });
                    }

                    for (size_t i = 0; i < count; i++) {
                        const auto& entry = entries[i];
                        auto& subDataPipelines = entry.subData->*pipelines;
                        if (subDataPipelines.graphPending)
                            UpdateGraphPipelines(entry, subDataPipelines, createConfig, hasGraphPipeline);
                        if (!subDataPipelines.configs[0].IsValid()) {
                            subDataPipelines.Set(createConfig(entry.subData, entry.mesh, nullptr));

                            if (entry.subData->material->HasGraph()) {
                                subDataPipelines.graphPending = true;
                                UpdateGraphPipelines(entry, subDataPipelines, createConfig, hasGraphPipeline);
                            }
                        }
                    }

                    // Simulated soft bodies read their vertices instead of the instance buffer. The variants are derived
                    // once the graph pipelines of all entries of the sub data are set, setting them clears the variants
                    for (size_t i = 0; i < count; i++) {
                        const auto& entry = entries[i];
                        auto& subDataPipelines = entry.subData->*pipelines;
                        if (entry.softBody && !subDataPipelines.configs[2].IsValid()) {
                            for (int32_t winding = 0; winding < 2; winding++) {
                                auto config = subDataPipelines.configs[winding];
                                config.RemoveMacro("INSTANCE_BUFFER");
                                config.AddMacro("SOFT_BODY");
                                subDataPipelines.configs[2 + winding] = config;
                            }
                        }
                    }

                    std::sort(entries.begin(), entries.begin() + count, [&](const Entry& entry0, const Entry& entry1) {
                        return GetConfig(entry0, pipelines).variantHash < GetConfig(entry1, pipelines).variantHash;
                        });

                }

                // fillPushConstants(entry, instances, usesGraph) writes the push constants of a sub data and returns them,
                // setLod(lod) changes the level of detail in them and returns whether they changed
                template<class FillPushConstants, class SetLod>
                void Draw(Graphics::CommandList* commandList, RenderList* renderList, const RenderList::Pass& pass,
                    Mesh::MeshSubDataPipelines Mesh::MeshSubData::* pipelines, const FillPushConstants& fillPushConstants,
                    const SetLod& setLod) {

                    Hash prevMesh = 0;
                    Hash prevHash = 0;
                    Ref<Graphics::Pipeline> currentPipeline;
                    const Graphics::PushConstantRange* constantsRange = nullptr;

                    // Variants whose shader the driver couldn't compile aren't drawn instead of binding an invalid pipeline
                    auto bindPipeline = [&](PipelineConfig& config) {
                        if (config.variantHash == prevHash)
                            return currentPipeline->isComplete;

                        currentPipeline = PipelineManager::GetPipeline(config);
                        prevHash = config.variantHash;
                        if (!currentPipeline->isComplete)
                            return false;

                        commandList->BindPipeline(currentPipeline);
                        constantsRange = currentPipeline->shader->GetPushConstantRange("constants");
                        return true;
                    };

                    for (size_t i = 0; i < count; i++) {
                        const auto& entry = entries[i];
                        auto& subDataPipelines = entry.subData->*pipelines;

                        RenderList::MeshInstances instances = {};
                        if (!entry.gpuDraw)
                            instances = entry.softBody ? pass.meshToSoftBodyInstancesMap.at(entry.meshId) :
                                pass.meshToInstancesMap.at(entry.meshId);

                        if (!bindPipeline(GetConfig(entry, pipelines)))
                            continue;

                        if (entry.meshId != prevMesh) {
                            entry.mesh->vertexArray.Bind(commandList);
                            prevMesh = entry.meshId;
                        }

                        auto& pushConstants = fillPushConstants(entry, instances, subDataPipelines.usesGraph);
                        commandList->PushConstants(constantsRange, &pushConstants);

                        if (entry.gpuDraw) {
                            renderList->DrawGPUCulled(commandList, *entry.gpuDraw);
                            continue;
                        }

                        auto beforeDraw = [&](uint32_t lod) {
                            if (setLod(lod))
                                commandList->PushConstants(constantsRange, &pushConstants);
                        };

                        RenderList::DrawInstances(commandList, *entry.subData, instances, false, beforeDraw);
                        if (instances.reversedWindingCount && bindPipeline(subDataPipelines.Get(entry.softBody, true))) {
                            commandList->PushConstants(constantsRange, &pushConstants);
                            RenderList::DrawInstances(commandList, *entry.subData, instances, true, beforeDraw);
                        }
                    }

                }

            private:
                static PipelineConfig& GetConfig(const Entry& entry, Mesh::MeshSubDataPipelines Mesh::MeshSubData::* pipelines) {

                    auto reversedWinding = entry.gpuDraw && entry.gpuDraw->type == RenderList::GPUDrawType::ReversedWinding;
                    return (entry.subData->*pipelines).Get(entry.softBody, reversedWinding);

                }

                // Switches to the pipelines of the graph once they are created
                template<class CreateConfig, class HasGraphPipeline>
                static void UpdateGraphPipelines(const Entry& entry, Mesh::MeshSubDataPipelines& subDataPipelines,
                    const CreateConfig& createConfig, const HasGraphPipeline& hasGraphPipeline) {

                    using PipelineState = MaterialGraph::PipelineCache::State;

                    const auto& graph = entry.subData->material->graph;
                    auto compiled = graph.IsLoaded() ? graph->GetCompiled() : nullptr;
                    if (!compiled || !compiled->valid || !hasGraphPipeline(*compiled)) {
                        subDataPipelines.graphPending = false;
                        return;
                    }

                    auto config = createConfig(entry.subData, entry.mesh, compiled.get());
                    auto reversedWindingConfig = config.GetReversedWindingVariant();

                    auto blocking = graph->blockingPipelineUsers > 0;
                    auto state = MaterialGraph::PipelineCache::Request(config, blocking);
                    auto reversedWindingState = MaterialGraph::PipelineCache::Request(reversedWindingConfig, blocking);
                    if (state == PipelineState::Pending || reversedWindingState == PipelineState::Pending)
                        return;

                    subDataPipelines.graphPending = false;
                    if (state == PipelineState::Failed || reversedWindingState == PipelineState::Failed)
                        return;

                    subDataPipelines.Set(config);
                    subDataPipelines.usesGraph = true;

                }

                std::vector<Entry> entries;
                size_t count = 0;

            };

        }

    }

}
