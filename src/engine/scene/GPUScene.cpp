#include "GPUScene.h"
#include "Scene.h"
#include "../profiler/CPUProfiler.h"

#include "common/TransformOrientation.h"
#include "graphics/GraphicsDevice.h"
#include "../profiler/GPUProfiler.h"
#include "pipeline/PipelineManager.h"

#include <glm/gtx/norm.hpp>

namespace Atlas::Scene {

    GPUScene::GPUScene(Scene* scene) : scene(scene) {

        scatterPipelineConfig = PipelineConfig("culling/scatter.csh");
        cullPipelineConfig = PipelineConfig("culling/cull.csh");
        cullRayTracingPipelineConfig = PipelineConfig("culling/cull.csh", { "RAY_TRACING_INSTANCES" });
        cullOcclusionPipelineConfig = PipelineConfig("culling/cull.csh", { "OCCLUSION" });
        drawsPipelineConfig = PipelineConfig("culling/draws.csh");

    }

    void GPUScene::Update(const std::vector<ECS::Entity>& dirtyEntities, const std::vector<ECS::Entity>& lastDirtyEntities) {

        auto& meshComponentPool = scene->entityManager.GetPool<MeshComponent>();
        auto& transformComponentPool = scene->entityManager.GetPool<TransformComponent>();

        updates.clear();
        updateIndices.clear();
        history.clear();
        refreshEntities.clear();

        {
            std::scoped_lock lock(meshDistancesMutex);
            for (size_t i = 0; i < std::min(visibleMeshIds.size(), slots.size()); i++) {
                if (slots[i].mesh.IsValid() && slots[i].mesh.GetID() == visibleMeshIds[i])
                    slots[i].drawEarly = visibleMeshes[i] > 0;
            }
            visibleMeshIds.clear();
        }

        if (!valid) {
            meshSlots.clear();
            freeSlots.clear();
            slots = std::vector<MeshSlot>(1);
            entityStates.clear();

            for (size_t i = 0; i < meshComponentPool.GetCount(); i++)
                refreshEntities.push_back(meshComponentPool[i]);

            // Instances of entities which are gone are only removed by clearing all of them
            clearInstances = true;
            valid = true;
        }
        else {
            // The matrix of the last frame changes one frame after the transform
            refreshEntities.insert(refreshEntities.end(), dirtyEntities.begin(), dirtyEntities.end());
            refreshEntities.insert(refreshEntities.end(), lastDirtyEntities.begin(), lastDirtyEntities.end());
        }

        // Components are looked up by entity index, the version doesn't matter
        std::sort(refreshEntities.begin(), refreshEntities.end(), [](ECS::Entity entity0, ECS::Entity entity1) {
            return ECS::EntityToIdx(entity0) < ECS::EntityToIdx(entity1);
            });
        auto last = std::unique(refreshEntities.begin(), refreshEntities.end(), [](ECS::Entity entity0, ECS::Entity entity1) {
            return ECS::EntityToIdx(entity0) == ECS::EntityToIdx(entity1);
            });
        refreshEntities.erase(last, refreshEntities.end());

        for (auto entity : refreshEntities) {
            auto entityIdx = ECS::EntityToIdx(entity);
            if (entityIdx >= entityStates.size())
                entityStates.resize(entityIdx + 1);

            auto meshComponent = meshComponentPool.TryGet(entity);
            auto transformComponent = transformComponentPool.TryGet(entity);

            uint32_t slotIdx = 0;
            if (meshComponent && transformComponent && meshComponent->mesh.IsValid())
                slotIdx = GetSlot(meshComponent->mesh);

            auto& state = entityStates[entityIdx];
            auto reversedWinding = slotIdx && Common::HasReversedWinding(transformComponent->globalMatrix);
            auto dontCull = slotIdx && meshComponent->dontCull;

            if (state.slot) {
                auto& oldSlot = slots[state.slot];
                oldSlot.reversedWindingCount -= state.reversedWinding ? 1 : 0;
                oldSlot.dontCullCount -= state.dontCull ? 1 : 0;
                if (state.slot != slotIdx)
                    RemoveFromSlot(entityIdx, state);
            }

            GPUInstance instance;
            if (slotIdx) {
                auto& slot = slots[slotIdx];
                if (state.slot != slotIdx) {
                    state.slot = slotIdx;
                    state.slotPosition = uint32_t(slot.entities.size());
                    slot.entities.push_back(entity);
                }
                else {
                    slot.entities[state.slotPosition] = entity;
                    instance.visibleLastFrame = keepVisibleLastFrame;
                }

                slot.reversedWindingCount += reversedWinding ? 1 : 0;
                slot.dontCullCount += dontCull ? 1 : 0;

                const auto& globalMatrix = transformComponent->globalMatrix;
                if (slot.hasBounds)
                    GrowBounds(slot, globalMatrix);

                instance.matrix = mat3x4(glm::transpose(globalMatrix));
                instance.meshIdx = slotIdx;
                instance.flags = (meshComponent->visible ? InstanceVisible : 0) |
                    (dontCull ? InstanceDontCull : 0) | (reversedWinding ? InstanceReversedWinding : 0);

                if (transformComponent->lastGlobalMatrix != globalMatrix) {
                    instance.historyIdx = uint32_t(history.size());
                    history.push_back(mat3x4(glm::transpose(transformComponent->lastGlobalMatrix)));
                }
            }

            state.reversedWinding = reversedWinding;
            state.dontCull = dontCull;

            updates.push_back(instance);
            updateIndices.push_back(entityIdx);
        }

        instanceCount = uint32_t(entityStates.size());

        UpdateMeshes();

        meshBuffer = uploadBufferPool.Upload(cullMeshes.data(), cullMeshes.size() * sizeof(GPUCullMesh));

    }

    void GPUScene::UpdateBuffers(Graphics::CommandList* commandList) {

        CPUProfiler::Scope profilerScope("GPU scene update");
        GPUProfiler::Scope gpuScope("GPU scene update");

        // The instances might still be read by the previous frame
        commandList->MemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT,
            VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        auto usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        auto instanceSize = sizeof(GPUInstance) * std::max(instanceCount, 1u);
        // Instances which are never refreshed are culled, so the whole buffer starts out zero
        uint32_t zero = 0;
        if (!instanceBuffer || instanceBuffer->size < instanceSize) {
            auto oldBuffer = instanceBuffer;
            // Some headroom, entities are added while playing
            EnsureBuffer(instanceBuffer, instanceSize + instanceSize / 4, usage);
            commandList->FillBuffer(instanceBuffer, &zero);
            if (oldBuffer && !clearInstances) {
                commandList->MemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
                commandList->CopyBuffer(oldBuffer, instanceBuffer, VkBufferCopy{ .size = oldBuffer->size });
            }
            clearInstances = false;
        }

        if (clearInstances) {
            commandList->FillBuffer(instanceBuffer, &zero);
            clearInstances = false;
        }

        EnsureBuffer(historyBuffer, sizeof(mat3x4) * std::max(history.size(), size_t(1)), usage);
        if (!history.empty()) {
            auto historyUploadBuffer = uploadBufferPool.Upload(history.data(), history.size() * sizeof(mat3x4));
            commandList->CopyBuffer(historyUploadBuffer, historyBuffer, VkBufferCopy{ .size = history.size() * sizeof(mat3x4) });
        }

        commandList->MemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        if (!updates.empty()) {
            auto pipeline = PipelineManager::GetPipeline(scatterPipelineConfig);
            commandList->BindPipeline(pipeline);

            auto updateBuffer = uploadBufferPool.Upload(updates.data(), updates.size() * sizeof(GPUInstance));
            auto updateIndexBuffer = uploadBufferPool.Upload(updateIndices.data(), updateIndices.size() * sizeof(uint32_t));

            commandList->BindBuffer(instanceBuffer, 3, 0);
            commandList->BindBuffer(updateBuffer, 3, 1);
            commandList->BindBuffer(updateIndexBuffer, 3, 2);

            auto updateCount = uint32_t(updates.size());
            commandList->PushConstants("constants", &updateCount);
            commandList->Dispatch((updateCount + 63) / 64, 1, 1);
        }

        commandList->MemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    }

    void GPUScene::FillPass(const Volume::Frustum& frustum, vec3 cameraLocation, const Ref<RenderList::Pass>& pass) {

        auto& meshComponentPool = scene->entityManager.GetPool<MeshComponent>();
        auto shadow = pass->type == RenderList::RenderPassType::Shadow;

        pass->frustum = frustum;
        pass->gpuDraws.clear();
        pass->gpuLateDraws.clear();

        for (uint32_t slotIdx = 1; slotIdx < uint32_t(slots.size()); slotIdx++) {
            const auto& slot = slots[slotIdx];
            if (!slot.drawable && !slot.skinned)
                continue;

            const auto& mesh = slot.mesh;
            if (shadow && !mesh->castShadow)
                continue;

            auto cullingDistance = shadow ? mesh->shadowDistanceCulling : mesh->distanceCulling;
            auto cullingDistanceSqr = cullingDistance * cullingDistance;

            if (slot.skinned) {
                for (auto entity : slot.entities) {
                    const auto& meshComponent = meshComponentPool.Get(entity);
                    auto distanceSqr = glm::distance2(meshComponent.aabb.GetCenter(), cameraLocation);
                    if (meshComponent.dontCull || (meshComponent.visible && distanceSqr <= cullingDistanceSqr &&
                        frustum.Intersects(meshComponent.aabb)))
                        pass->Add(entity, meshComponent);
                }
                continue;
            }

            auto closestPoint = glm::clamp(cameraLocation, slot.bounds.min, slot.bounds.max);
            auto minDistanceSqr = glm::distance2(closestPoint, cameraLocation);
            if (!slot.dontCullCount && (!frustum.Intersects(slot.bounds) || minDistanceSqr > cullingDistanceSqr))
                continue;

            // Only draw what the instances can turn into at their distances
            auto hasMeshes = true, hasImpostors = false;
            if (slot.impostor) {
                auto farthestPoint = glm::max(glm::abs(cameraLocation - slot.bounds.min),
                    glm::abs(cameraLocation - slot.bounds.max));
                auto maxDistanceSqr = glm::length2(farthestPoint);
                auto impostorDistance = shadow ? mesh->impostorShadowDistance : mesh->impostorDistance;
                hasMeshes = minDistanceSqr < impostorDistance * impostorDistance;
                hasImpostors = maxDistanceSqr >= impostorDistance * impostorDistance;
            }

            auto occluded = !shadow && scene->occlusionCulling;
            auto addDraw = [&](Mesh::MeshSubData* subData, RenderList::GPUDrawType type) {
                if (occluded)
                    pass->gpuLateDraws.push_back({ subData, mesh.Get().get(), mesh.GetID(), slotIdx, type });
                if (!occluded || slot.drawEarly || slot.dontCullCount)
                    pass->gpuDraws.push_back({ subData, mesh.Get().get(), mesh.GetID(), slotIdx, type });
            };

            // Meshes without resident geometry are drawn as impostors only
            if (!slot.geometry) {
                hasMeshes = false;
                hasImpostors = true;
            }

            if (hasMeshes) {
                for (auto& subData : mesh->data.subData) {
                    if (!subData.material.IsLoaded())
                        continue;

                    addDraw(&subData, RenderList::GPUDrawType::DefaultWinding);
                    if (slot.reversedWindingCount)
                        addDraw(&subData, RenderList::GPUDrawType::ReversedWinding);
                }
            }

            if (hasImpostors)
                addDraw(nullptr, RenderList::GPUDrawType::Impostor);
        }

    }

    void GPUScene::Cull(RenderList& renderList, vec3 cameraLocation, float focalLength, Graphics::CommandList* commandList) {

        lastFocalLength = focalLength;

        auto device = Graphics::GraphicsDevice::DefaultDevice;
        auto slotCount = uint32_t(cullMeshes.size());

        std::vector<GPUCullView> views;
        std::vector<GPUDrawRequest> requests;
        const auto& passes = renderList.gpuCulledPasses;
        auto occlusionView = 0xFFFFFFFFu;
        lateRequestCount = 0;
        for (size_t i = 0; i < passes.size(); i++) {
            const auto& pass = passes[i];
            auto viewIdx = uint32_t(i);

            GPUCullView view = {
                .shadow = pass->type == RenderList::RenderPassType::Shadow ? 1u : 0u,
                .indexOffset = viewIdx * totalCapacity,
                .impostorIndexOffset = viewIdx * impostorCapacity,
                .counterOffset = viewIdx * slotCount * drawTypeCount
            };
            auto planes = pass->frustum.GetPlanes();
            std::copy(planes.begin(), planes.end(), view.planes);
            views.push_back(view);

            for (auto& draw : pass->gpuDraws) {
                draw.command = uint32_t(requests.size());
                requests.push_back(GPUDrawRequest {
                    .meshIdx = draw.slot,
                    .view = viewIdx,
                    .type = uint32_t(draw.type),
                    .indexCount = draw.subData ? draw.subData->indicesCount : 0,
                    .firstIndex = draw.subData ? draw.subData->indicesOffset : 0
                    });
            }

            if (!pass->gpuLateDraws.empty())
                occlusionView = viewIdx;
        }

        // The draws after the occlusion test have a view of their own
        auto cullViewCount = uint32_t(views.size());
        occlusionViewOffset = occlusionView != 0xFFFFFFFFu ?
            VkDeviceSize(sizeof(uint32_t) * occlusionView * slotCount * drawTypeCount) : 0;
        lateFirstRequest = uint32_t(requests.size());
        if (occlusionView != 0xFFFFFFFFu) {
            lateView = cullViewCount;
            GPUCullView view = views[occlusionView];
            view.indexOffset = lateView * totalCapacity;
            view.impostorIndexOffset = lateView * impostorCapacity;
            view.counterOffset = lateView * slotCount * drawTypeCount;
            views.push_back(view);

            for (auto& draw : passes[occlusionView]->gpuLateDraws) {
                draw.command = uint32_t(requests.size());
                requests.push_back(GPUDrawRequest {
                    .meshIdx = draw.slot,
                    .view = lateView,
                    .type = uint32_t(draw.type),
                    .indexCount = draw.subData ? draw.subData->indicesCount : 0,
                    .firstIndex = draw.subData ? draw.subData->indicesOffset : 0
                    });
            }
            lateRequestCount = uint32_t(requests.size()) - lateFirstRequest;
        }

        EnsureCullBuffers(views.size(), requests.size(), lateRequestCount > 0);
        renderList.gpuCommandBuffer = commandBuffer;

        // Without any draw the instances are still culled for the streaming, streamed out meshes have no draws
        if (requests.empty() && instanceCount == 0)
            return;

        CPUProfiler::Scope profilerScope("GPU culling");
        GPUProfiler::Scope gpuScope("GPU culling");

        auto viewBuffer = uploadBufferPool.Upload(views.data(), views.size() * sizeof(GPUCullView));
        auto requestBuffer = uploadBufferPool.Upload(requests.data(), requests.size() * sizeof(GPUDrawRequest));

        cullViewBuffer = viewBuffer;
        cullRequestBuffer = requestBuffer;
        cullCameraLocation = cameraLocation;
        cullFocalLength = focalLength;

        // A scene that is culled multiple times in a frame reads back the distances of its first culling only
        // The GPU finished the copy into this frame copy of the readback buffer FRAME_DATA_COUNT frames ago
        auto frameIndex = device->GetFrameIndex();
        auto readbackDistances = instanceCount > 0 && distanceFrameIndex != frameIndex;
        readbackVisibility = readbackDistances && lateRequestCount > 0;
        if (readbackDistances) {
            distanceFrameIndex = frameIndex;

            auto& frameVisibilityIds = visibilityMeshIds[frameIndex % FRAME_DATA_COUNT];
            if (!frameVisibilityIds.empty()) {
                visibilityReadbackBuffer->Invalidate();
                auto counters = static_cast<uint32_t*>(visibilityReadbackBuffer->Map());

                // The counters of each slot of the first and the late draws
                auto slotCounters = size_t(drawTypeCount);
                auto copiedSlots = frameVisibilityIds.size();
                std::scoped_lock lock(meshDistancesMutex);
                visibleMeshes.resize(copiedSlots);
                for (size_t i = 0; i < copiedSlots; i++) {
                    uint32_t count = 0;
                    for (size_t j = 0; j < slotCounters; j++)
                        count += counters[i * slotCounters + j] + counters[(copiedSlots + i) * slotCounters + j];
                    visibleMeshes[i] = count > 0 ? 1 : 0;
                }
                visibleMeshIds = std::move(frameVisibilityIds);

                visibilityReadbackBuffer->Unmap();
            }
            frameVisibilityIds.clear();

            auto visibilitySize = 2 * sizeof(uint32_t) * drawTypeCount * std::max(size_t(slotCount), size_t(1));
            if (!visibilityReadbackBuffer || visibilityReadbackBuffer->size < visibilitySize) {
                Graphics::BufferDesc desc = {
                    .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    .domain = Graphics::BufferDomain::Host,
                    .hostAccess = Graphics::BufferHostAccess::Random,
                    .size = visibilitySize * 2
                };
                visibilityReadbackBuffer = device->CreateMultiBuffer(desc);

                // Copies that are still in flight went into the old buffer
                for (auto& meshIds : visibilityMeshIds)
                    meshIds.clear();
            }

            auto& frameMeshIds = distanceMeshIds[frameIndex % FRAME_DATA_COUNT];
            if (!frameMeshIds.empty()) {
                meshDistanceReadbackBuffer->Invalidate();
                auto distances = static_cast<float*>(meshDistanceReadbackBuffer->Map());

                // Two distances per slot, see culling/cull.csh
                std::scoped_lock lock(meshDistancesMutex);
                meshDistances.distances.resize(frameMeshIds.size());
                meshDistances.positionDistances.resize(frameMeshIds.size());
                for (size_t i = 0; i < frameMeshIds.size(); i++) {
                    meshDistances.distances[i] = distances[2 * i];
                    meshDistances.positionDistances[i] = distances[2 * i + 1];
                }
                meshDistances.meshIds = std::move(frameMeshIds);
                hasMeshDistances = true;

                meshDistanceReadbackBuffer->Unmap();
            }
            frameMeshIds.clear();

            auto readbackSize = 2 * sizeof(uint32_t) * std::max(size_t(slotCount), size_t(1));
            if (!meshDistanceReadbackBuffer || meshDistanceReadbackBuffer->size < readbackSize) {
                Graphics::BufferDesc desc = {
                    .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    .domain = Graphics::BufferDomain::Host,
                    .hostAccess = Graphics::BufferHostAccess::Random,
                    .size = readbackSize * 2
                };
                meshDistanceReadbackBuffer = device->CreateMultiBuffer(desc);

                // Copies that are still in flight went into the old buffer
                for (auto& meshIds : distanceMeshIds)
                    meshIds.clear();
            }
        }

        // The previous frame might still draw with the commands and indices
        commandList->MemoryBarrier(VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        uint32_t zero = 0;
        commandList->FillBuffer(counterBuffer, &zero);
        if (lateRequestCount > 0)
            commandList->FillBuffer(occlusionInstanceBuffer, &zero);
        if (readbackDistances) {
            auto maxDistance = 0xFFFFFFFFu;
            commandList->FillBuffer(meshDistanceBuffer, &maxDistance);
        }

        commandList->MemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        if (instanceCount > 0) {
            auto pipeline = PipelineManager::GetPipeline(cullPipelineConfig);
            commandList->BindPipeline(pipeline);

            BindCullBuffers(viewBuffer, commandList);
            commandList->BindBuffer(meshDistanceBuffer, 3, 12);
            commandList->BindBuffer(occlusionInstanceBuffer, 3, 14);

            auto constants = CullPushConstants {
                .cameraLocation = vec4(cameraLocation, 1.0f),
                .instanceCount = instanceCount,
                .viewCount = cullViewCount,
                .focalLength = focalLength,
                .occlusionView = occlusionView
            };
            commandList->PushConstants("constants", &constants);
            commandList->Dispatch((instanceCount + 63) / 64, 1, 1);

            commandList->MemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        }

        if (lateFirstRequest > 0)
            WriteDrawCommands(0, lateFirstRequest, commandList);

        if (readbackDistances) {
            commandList->MemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

            VkBufferCopy copy = { .size = 2 * sizeof(uint32_t) * slotCount };
            commandList->CopyBuffer(meshDistanceBuffer, meshDistanceReadbackBuffer, copy);

            commandList->MemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);

            distanceMeshIds[frameIndex % FRAME_DATA_COUNT] = cullMeshIds;
        }

    }

    void GPUScene::CullForStreaming(vec3 cameraLocation, float focalLength, Graphics::CommandList* commandList) {

        RenderList renderList;
        Cull(renderList, cameraLocation, focalLength, commandList);

    }

    float GPUScene::GetFocalLength() const {

        return lastFocalLength;

    }

    void GPUScene::CullOcclusion(const Ref<Texture::Texture2D>& hiZTexture, ivec2 depthResolution, int32_t hiZLevelCount,
        Graphics::CommandList* commandList) {

        CPUProfiler::Scope profilerScope("GPU occlusion culling");
        GPUProfiler::Scope gpuScope("GPU occlusion culling");

        // The first draws read the commands and the indices, which are written next to them
        commandList->MemoryBarrier(VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        auto pipeline = PipelineManager::GetPipeline(cullOcclusionPipelineConfig);
        commandList->BindPipeline(pipeline);

        BindCullBuffers(cullViewBuffer, commandList);
        commandList->BindImage(hiZTexture->image, hiZTexture->sampler, 3, 13);
        commandList->BindBuffer(occlusionInstanceBuffer, 3, 14);

        auto constants = CullPushConstants {
            .cameraLocation = vec4(cullCameraLocation, 1.0f),
            .instanceCount = instanceCount,
            .viewCount = 1,
            .focalLength = cullFocalLength,
            .firstView = lateView,
            .depthResolution = depthResolution,
            .hiZLevelCount = hiZLevelCount
        };
        commandList->PushConstants("constants", &constants);
        commandList->DispatchIndirect(occlusionInstanceBuffer);

        commandList->MemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        WriteDrawCommands(lateFirstRequest, lateRequestCount, commandList);

        if (readbackVisibility) {
            commandList->MemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

            auto slotCount = uint32_t(cullMeshes.size());
            auto viewSize = VkDeviceSize(sizeof(uint32_t) * drawTypeCount * slotCount);
            commandList->CopyBuffer(counterBuffer, visibilityReadbackBuffer,
                VkBufferCopy{ .srcOffset = occlusionViewOffset, .dstOffset = 0, .size = viewSize });
            commandList->CopyBuffer(counterBuffer, visibilityReadbackBuffer,
                VkBufferCopy{ .srcOffset = lateView * viewSize, .dstOffset = viewSize, .size = viewSize });

            commandList->MemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);

            visibilityMeshIds[Graphics::GraphicsDevice::DefaultDevice->GetFrameIndex() % FRAME_DATA_COUNT] = cullMeshIds;
        }

    }

    void GPUScene::WriteDrawCommands(uint32_t firstRequest, uint32_t requestCount, Graphics::CommandList* commandList) {

        auto pipeline = PipelineManager::GetPipeline(drawsPipelineConfig);
        commandList->BindPipeline(pipeline);

        commandList->BindBuffer(meshBuffer, 3, 0);
        commandList->BindBuffer(cullViewBuffer, 3, 1);
        commandList->BindBuffer(counterBuffer, 3, 2);
        commandList->BindBuffer(cullRequestBuffer, 3, 3);
        commandList->BindBuffer(commandBuffer, 3, 4);

        auto constants = DrawsPushConstants { .firstRequest = firstRequest, .requestCount = requestCount };
        commandList->PushConstants("constants", &constants);
        commandList->Dispatch((requestCount + 63) / 64, 1, 1);

        commandList->MemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);

    }

    void GPUScene::TakeMeshDistances(MeshDistances& distances) {

        std::scoped_lock lock(meshDistancesMutex);
        if (!hasMeshDistances)
            return;

        std::swap(distances, meshDistances);
        hasMeshDistances = false;

    }

    void GPUScene::WriteRayTracingInstances(RayTracing::RayTracingWorld& rayTracingWorld, vec3 cameraLocation,
        float focalLength, Graphics::CommandList* commandList) {

        auto pendingInstances = rayTracingWorld.GetPendingInstances();
        if (!pendingInstances)
            return;

        CPUProfiler::Scope profilerScope("Ray tracing instances");
        GPUProfiler::BeginQuery("Ray tracing instances");

        rayTracingWorld.PrepareInstances(commandList);

        commandList->MemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        if (instanceCount > 0 && pendingInstances->gpuInstanceCount > 0) {
            auto pipeline = PipelineManager::GetPipeline(cullRayTracingPipelineConfig);
            commandList->BindPipeline(pipeline);

            // Without any view only the ray tracing instances are written, the cull buffers just need to be bound
            EnsureCullBuffers(0, 0);
            auto viewBuffer = uploadBufferPool.Upload(nullptr, 0);
            BindCullBuffers(viewBuffer, commandList);
            commandList->BindBuffer(historyBuffer, 3, 6);
            rayTracingWorld.BindInstanceOutputs(commandList);

            auto constants = CullPushConstants {
                .cameraLocation = vec4(cameraLocation, 1.0f),
                .instanceCount = instanceCount,
                .viewCount = 0,
                .maxRayTracingInstanceCount = pendingInstances->instanceBound,
                .includeObjectHistory = pendingInstances->includeObjectHistory ? 1u : 0u,
                .focalLength = focalLength
            };
            commandList->PushConstants("constants", &constants);
            commandList->Dispatch((instanceCount + 63) / 64, 1, 1);
        }

        GPUProfiler::EndQuery();

        rayTracingWorld.FinishInstances(commandList);

    }

    void GPUScene::Bind(Graphics::CommandList* commandList) const {

        commandList->BindBuffer(instanceBuffer, 1, 20);
        commandList->BindBuffer(historyBuffer, 1, 21);
        commandList->BindBuffer(indexBuffer, 1, 22);
        commandList->BindBuffer(impostorIndexBuffer, 1, 23);

    }

    void GPUScene::Invalidate() {

        valid = false;

    }

    uint32_t GPUScene::GetSlot(const ResourceHandle<Mesh::Mesh>& mesh) {

        auto it = meshSlots.find(mesh.GetID());
        if (it != meshSlots.end())
            return it->second;

        uint32_t slotIdx;
        if (!freeSlots.empty()) {
            slotIdx = freeSlots.back();
            freeSlots.pop_back();
        }
        else {
            slotIdx = uint32_t(slots.size());
            slots.emplace_back();
        }

        slots[slotIdx].mesh = mesh;
        meshSlots[mesh.GetID()] = slotIdx;

        return slotIdx;

    }

    void GPUScene::RemoveFromSlot(uint32_t entityIdx, EntityState& state) {

        auto slotIdx = state.slot;
        auto& slot = slots[slotIdx];

        auto lastEntity = slot.entities.back();
        slot.entities[state.slotPosition] = lastEntity;
        entityStates[ECS::EntityToIdx(lastEntity)].slotPosition = state.slotPosition;
        slot.entities.pop_back();

        state.slot = 0;

        if (slot.entities.empty()) {
            meshSlots.erase(slot.mesh.GetID());
            slot = MeshSlot();
            freeSlots.push_back(slotIdx);
        }

    }

    void GPUScene::GrowBounds(MeshSlot& slot, const mat4& globalMatrix) {

        auto bounds = slot.mesh->GetRenderBounds().Transform(globalMatrix);
        // Impostors are chosen by the distance to the origin of the instance
        bounds.Grow(vec3(globalMatrix[3]));
        if (slot.hasBounds)
            slot.bounds.Grow(bounds);
        else
            slot.bounds = bounds;
        slot.hasBounds = true;

    }

    void GPUScene::UpdateMeshes() {

        auto& transformComponentPool = scene->entityManager.GetPool<TransformComponent>();

        cullMeshes.resize(slots.size());
        cullMeshIds.resize(slots.size());
        totalCapacity = 0;
        impostorCapacity = 0;

        for (size_t slotIdx = 0; slotIdx < slots.size(); slotIdx++) {
            auto& slot = slots[slotIdx];

            slot.drawable = false;
            slot.skinned = false;
            slot.impostor = false;
            slot.geometry = false;

            auto loaded = slotIdx > 0 && !slot.entities.empty() && slot.mesh.IsLoaded();
            if (loaded) {
                slot.skinned = slot.mesh->data.IsSkinned();
                slot.geometry = !slot.skinned && slot.mesh->IsGeometryResident();
                slot.impostor = !slot.skinned && slot.mesh->impostor.IsLoaded() && slot.mesh->impostor->isGenerated &&
                    slot.mesh->impostor->IsResident();
                // Without resident geometry the instances are drawn as impostors
                slot.drawable = slot.geometry || slot.impostor;
            }

            // The mesh might have been loaded after its instances were added
            if (loaded && !slot.skinned && !slot.hasBounds) {
                for (auto entity : slot.entities)
                    GrowBounds(slot, transformComponentPool.Get(entity).globalMatrix);
            }

            // Meshes which aren't drawable are culled as well, the streaming needs their distances
            GPUCullMesh cullMesh = {};
            if (loaded && !slot.skinned) {
                const auto& mesh = slot.mesh;
                auto count = slot.drawable ? uint32_t(slot.entities.size()) : 0u;

                slot.regionOffset = totalCapacity;
                slot.impostorRegionOffset = impostorCapacity;
                totalCapacity += count;
                impostorCapacity += slot.impostor ? count : 0;

                auto renderBounds = mesh->GetRenderBounds();

                cullMesh = GPUCullMesh {
                    .aabbMin = vec4(renderBounds.min, mesh->distanceCulling),
                    .aabbMax = vec4(renderBounds.max, mesh->shadowDistanceCulling),
                    .impostorDistance = mesh->impostorDistance,
                    .impostorShadowDistance = mesh->impostorShadowDistance,
                    .flags = (slot.drawable ? MeshDrawable : 0u) | (mesh->castShadow ? MeshCastShadow : 0u) |
                        (slot.impostor ? MeshImpostor : 0u) | (slot.geometry ? MeshGeometry : 0u) |
                        (slot.drawEarly || slot.dontCullCount ? MeshDrawEarly : 0u),
                    .minScreenSize = mesh->minScreenSize,
                    .regionOffset = slot.regionOffset,
                    .regionCapacity = count,
                    .impostorRegionOffset = slot.impostorRegionOffset
                };
            }

            cullMeshes[slotIdx] = cullMesh;
            cullMeshIds[slotIdx] = loaded && !slot.skinned ? slot.mesh.GetID() : 0;
        }

    }

    void GPUScene::EnsureCullBuffers(size_t viewCount, size_t requestCount, bool occlusion) {

        auto usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        viewCount = std::max(viewCount, size_t(1));
        auto slotCount = cullMeshes.size();
        EnsureBuffer(counterBuffer, sizeof(uint32_t) * drawTypeCount * viewCount * slotCount,
            usage | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        EnsureBuffer(indexBuffer, sizeof(uint32_t) * std::max(viewCount * totalCapacity, size_t(1)), usage);
        EnsureBuffer(impostorIndexBuffer, sizeof(uint32_t) * std::max(viewCount * impostorCapacity, size_t(1)), usage);
        EnsureBuffer(commandBuffer, RenderList::gpuDrawCommandSize * std::max(requestCount, size_t(1)),
            usage | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
        EnsureBuffer(meshDistanceBuffer, 2 * sizeof(uint32_t) * std::max(slotCount, size_t(1)),
            usage | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        EnsureBuffer(occlusionInstanceBuffer, sizeof(uint32_t) * (4 + (occlusion ? size_t(instanceCount) : size_t(1))),
            usage | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
        // In case no update was recorded yet
        EnsureBuffer(instanceBuffer, sizeof(GPUInstance), usage | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        EnsureBuffer(historyBuffer, sizeof(mat3x4), usage | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    }

    void GPUScene::BindCullBuffers(const Ref<Graphics::Buffer>& viewBuffer, Graphics::CommandList* commandList) const {

        commandList->BindBuffer(instanceBuffer, 3, 0);
        commandList->BindBuffer(meshBuffer, 3, 1);
        commandList->BindBuffer(viewBuffer, 3, 2);
        commandList->BindBuffer(counterBuffer, 3, 3);
        commandList->BindBuffer(indexBuffer, 3, 4);
        commandList->BindBuffer(impostorIndexBuffer, 3, 5);

    }

    void GPUScene::EnsureBuffer(Ref<Graphics::Buffer>& buffer, size_t size, VkBufferUsageFlags usage) {

        if (buffer && buffer->size >= size)
            return;

        auto device = Graphics::GraphicsDevice::DefaultDevice;
        Graphics::BufferDesc desc = {
            .usageFlags = usage,
            .domain = Graphics::BufferDomain::Device,
            .size = size
        };
        buffer = device->CreateBuffer(desc);

    }

}
