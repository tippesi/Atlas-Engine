#pragma once

#include "../../System.h"
#include "RenderList.h"
#include "../../pipeline/PipelineManager.h"
#include "../../materialgraph/PipelineCache.h"
#include "../../graphics/UploadBufferPool.h"
#include "../../graphics/GraphicsDevice.h"

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
                    // Lower orders are drawn first
                    uint32_t order = 0;
                };

                // Collects the host instances (if hostInstances) and the GPU draws, creates their pipelines with
                // createConfig(subData, mesh, graph) and sorts them by getOrder(config, usesGraph). Graphs only have a
                // pipeline of their own if hasGraphPipeline(graph)
                template<class CreateConfig, class HasGraphPipeline, class GetOrder>
                void Prepare(RenderList* renderList, const RenderList::Pass& pass, std::span<const RenderList::GPUDraw> gpuDraws,
                    bool hostInstances, Mesh::MeshSubDataPipelines Mesh::MeshSubData::* pipelines, const CreateConfig& createConfig,
                    const HasGraphPipeline& hasGraphPipeline, const GetOrder& getOrder) {

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

                    for (size_t i = 0; i < count; i++) {
                        auto& entry = entries[i];
                        auto& subDataPipelines = entry.subData->*pipelines;
                        if (subDataPipelines.order == Mesh::MeshSubDataPipelines::invalidOrder)
                            subDataPipelines.order = getOrder(subDataPipelines.configs[0], subDataPipelines.usesGraph);
                        entry.order = subDataPipelines.order;
                    }

                    // The draws of the GPU culling keep the order of their commands, the ones of a pipeline can be merged
                    std::sort(entries.begin(), entries.begin() + count, [&](const Entry& entry0, const Entry& entry1) {
                        if (entry0.order != entry1.order)
                            return entry0.order < entry1.order;
                        auto hash0 = GetConfig(entry0, pipelines).variantHash, hash1 = GetConfig(entry1, pipelines).variantHash;
                        if (hash0 != hash1)
                            return hash0 < hash1;
                        return GetCommand(entry0) < GetCommand(entry1);
                        });

                }

                // Copies fillDrawData(entry) of each draw into device memory, outside of rendering. Following GPU draws
                // with the same pipeline and geometry buffers become one multi draw if the pipeline reads the draw data
                template<class FillDrawData>
                void PrepareDrawData(Graphics::CommandList* commandList, Mesh::MeshSubDataPipelines Mesh::MeshSubData::* pipelines,
                    const FillDrawData& fillDrawData) {

                    using DrawData = decltype(fillDrawData(entries[0]));

                    // Each command of a multi draw has its data, host culled draws have a single one
                    runs.assign(count, Run());
                    drawData.clear();
                    for (size_t i = 0; i < count; i++) {
                        const auto& entry = entries[i];
                        auto& run = runs[i];
                        run.entryCount = 1;
                        run.firstCommand = entry.gpuDraw ? entry.gpuDraw->command : 0;
                        run.commandCount = entry.gpuDraw ? entry.gpuDraw->lodCount : 1;
                        run.firstDraw = uint32_t(drawData.size() / sizeof(DrawData));

                        // The commands of a GPU draw are its levels of detail
                        auto addDrawData = [&](const Entry& drawEntry, uint32_t drawCount) {
                            auto data = fillDrawData(drawEntry);
                            for (uint32_t j = 0; j < drawCount; j++) {
                                if constexpr (requires { data.lod; })
                                    data.lod = j;
                                drawData.insert(drawData.end(), reinterpret_cast<const uint8_t*>(&data),
                                    reinterpret_cast<const uint8_t*>(&data) + sizeof(DrawData));
                            }
                        };
                        addDrawData(entry, run.commandCount);
                        auto& config = GetConfig(entry, pipelines);
                        if (!entry.gpuDraw || !(entry.subData->*pipelines).hasDrawData)
                            continue;

                        while (i + 1 < count) {
                            const auto& next = entries[i + 1];
                            auto follows = next.gpuDraw && next.gpuDraw->command == run.firstCommand + run.commandCount;
                            if (!follows || GetConfig(next, pipelines).variantHash != config.variantHash ||
                                next.mesh->vertexBuffer.buffer != entry.mesh->vertexBuffer.buffer ||
                                next.mesh->indexBuffer.buffer != entry.mesh->indexBuffer.buffer)
                                break;

                            addDrawData(next, next.gpuDraw->lodCount);
                            run.entryCount++;
                            run.commandCount += next.gpuDraw->lodCount;
                            i++;
                            runs[i].entryCount = 0;
                        }
                    }

                    if (drawData.empty())
                        return;

                    // Passes without draws of the GPU culling (e.g. of local lights) are small and many, they read their
                    // data from host memory without a copy
                    auto hasGPUDraws = std::any_of(entries.begin(), entries.begin() + count,
                        [](const Entry& entry) { return entry.gpuDraw != nullptr; });
                    if (!hasGPUDraws) {
                        drawDataBuffer = uploadPool.Upload(drawData.data(), drawData.size());
                        return;
                    }

                    // The passes of a frame append their data, so the copy doesn't wait for the draws of the passes
                    // before. Read for every vertex and fragment, host memory would be read over the bus
                    auto device = Graphics::GraphicsDevice::DefaultDevice;
                    auto frameIndex = device->GetFrameIndex();
                    auto& frame = frameDrawData[frameIndex % FRAME_DATA_COUNT];
                    if (frame.frameIndex != frameIndex) {
                        frame.frameIndex = frameIndex;
                        frame.offset = 0;
                    }

                    auto size = drawData.size();
                    if (!frame.buffer || frame.offset + size > frame.buffer->size) {
                        Graphics::BufferDesc desc = {
                            .usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                            .domain = Graphics::BufferDomain::Device,
                            .size = 2 * (frame.offset + size)
                        };
                        frame.buffer = device->CreateBuffer(desc);
                        frame.offset = 0;
                    }

                    auto firstDraw = uint32_t(frame.offset / sizeof(DrawData));
                    for (size_t i = 0; i < count; i++)
                        runs[i].firstDraw += firstDraw;

                    commandList->CopyBuffer(uploadPool.Upload(drawData.data(), size), frame.buffer,
                        VkBufferCopy{ .dstOffset = frame.offset, .size = size });
                    commandList->MemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

                    frame.offset += size;
                    drawDataBuffer = frame.buffer;

                }

                // Draws after PrepareDrawData with fillPushConstants(entry, instances, usesGraph), setLod(lod) returns
                // whether it changed them. Shaders read the draw data at firstDraw + gl_DrawID of drawDataBinding in set 3.
                // Returns false if pipelines of some draws weren't ready
                template<class FillPushConstants, class SetLod>
                bool Draw(Graphics::CommandList* commandList, RenderList* renderList, const RenderList::Pass& pass,
                    Mesh::MeshSubDataPipelines Mesh::MeshSubData::* pipelines, const FillPushConstants& fillPushConstants,
                    const SetLod& setLod, uint32_t drawDataBinding) {

                    if (drawDataBuffer)
                        commandList->BindBuffer(drawDataBuffer, 3, drawDataBinding);

                    // Draws of the GPU culling only bind the shared buffers of their chunks, the others their own ones
                    const void* prevVertexBuffer = nullptr;
                    const void* prevIndexBuffer = nullptr;
                    bool prevGeometryOnly = false;
                    Hash prevHash = 0;
                    Ref<Graphics::Pipeline> currentPipeline;
                    const Graphics::PushConstantRange* constantsRange = nullptr;
                    bool drawnAll = true;

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

                        const auto& run = runs[i];
                        if (run.entryCount == 0)
                            continue;

                        if (!bindPipeline(GetConfig(entry, pipelines))) {
                            drawnAll = false;
                            i += run.entryCount - 1;
                            continue;
                        }

                        // The commands of the GPU culling and the host draws of pooled meshes select the geometry in the
                        // shared buffers of the geometry pool. Simulated soft bodies index their vertices with gl_VertexIndex
                        const auto& range = entry.mesh->GetGeometryRange();
                        bool geometryOnly = entry.gpuDraw != nullptr || range && !entry.softBody;
                        const void* vertexBuffer = geometryOnly ?
                            static_cast<const void*>(entry.mesh->vertexBuffer.buffer.get()) : entry.mesh;
                        const void* indexBuffer = geometryOnly ? entry.mesh->indexBuffer.buffer.get() : nullptr;
                        if (vertexBuffer != prevVertexBuffer || indexBuffer != prevIndexBuffer || geometryOnly != prevGeometryOnly) {
                            entry.mesh->vertexArray.Bind(commandList, !geometryOnly);
                            prevVertexBuffer = vertexBuffer;
                            prevIndexBuffer = indexBuffer;
                            prevGeometryOnly = geometryOnly;
                        }

                        auto& pushConstants = fillPushConstants(entry, instances, subDataPipelines.usesGraph);
                        pushConstants.firstDraw = run.firstDraw;
                        commandList->PushConstants(constantsRange, &pushConstants);

                        if (entry.gpuDraw) {
                            commandList->DrawIndexedIndirect(renderList->gpuCommandBuffer,
                                run.firstCommand * RenderList::gpuDrawCommandSize, run.commandCount, RenderList::gpuDrawCommandSize);
                            i += run.entryCount - 1;
                            continue;
                        }

                        auto beforeDraw = [&](uint32_t lod) {
                            if (setLod(lod))
                                commandList->PushConstants(constantsRange, &pushConstants);
                        };

                        auto firstIndex = geometryOnly ? range->firstIndex : 0u;
                        auto vertexOffset = geometryOnly ? int32_t(range->firstVertex) : 0;
                        RenderList::DrawInstances(commandList, *entry.subData, instances, false, firstIndex, vertexOffset, beforeDraw);
                        if (!instances.reversedWindingCount)
                            continue;
                        if (!bindPipeline(subDataPipelines.Get(entry.softBody, true))) {
                            drawnAll = false;
                            continue;
                        }
                        commandList->PushConstants(constantsRange, &pushConstants);
                        RenderList::DrawInstances(commandList, *entry.subData, instances, true, firstIndex, vertexOffset, beforeDraw);
                    }

                    return drawnAll;

                }

            private:
                // Entries drawn together, the run is stored at its first entry and empty at the others
                struct Run {
                    uint32_t entryCount = 0;
                    uint32_t firstCommand = 0;
                    uint32_t commandCount = 0;
                    uint32_t firstDraw = 0;
                };

                static uint32_t GetCommand(const Entry& entry) {

                    return entry.gpuDraw ? entry.gpuDraw->command : 0;

                }

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
                    subDataPipelines.graphAnimatesShadow = compiled->animatesShadow;
                    subDataPipelines.graphReadsTexCoords = compiled->shadowReadsTexCoords;

                }

                std::vector<Entry> entries;
                size_t count = 0;

                struct FrameDrawData {
                    int32_t frameIndex = -1;
                    size_t offset = 0;
                    Ref<Graphics::Buffer> buffer;
                };

                std::vector<Run> runs;
                std::vector<uint8_t> drawData;
                Graphics::UploadBufferPool uploadPool;
                FrameDrawData frameDrawData[FRAME_DATA_COUNT];
                Ref<Graphics::Buffer> drawDataBuffer;

            };

        }

    }

}
