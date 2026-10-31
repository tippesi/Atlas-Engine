#include "GPUScene.h"
#include "Scene.h"

#include "common/TransformOrientation.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/Profiler.h"
#include "pipeline/PipelineManager.h"

#include <glm/gtx/norm.hpp>

namespace Atlas::Scene {

    GPUScene::GPUScene(Scene* scene) : scene(scene) {

        scatterPipelineConfig = PipelineConfig("culling/scatter.csh");
        cullPipelineConfig = PipelineConfig("culling/cull.csh");
        cullRayTracingPipelineConfig = PipelineConfig("culling/cull.csh", { "RAY_TRACING_INSTANCES" });
        drawsPipelineConfig = PipelineConfig("culling/draws.csh");

    }

    void GPUScene::Update(const std::vector<ECS::Entity>& dirtyEntities, const std::vector<ECS::Entity>& lastDirtyEntities) {

        auto& meshComponentPool = scene->entityManager.GetPool<MeshComponent>();
        auto& transformComponentPool = scene->entityManager.GetPool<TransformComponent>();

        updates.clear();
        updateIndices.clear();
        history.clear();
        refreshEntities.clear();

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

        Graphics::Profiler::BeginQuery("GPU scene update");

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

        Graphics::Profiler::EndQuery();

    }

    void GPUScene::FillPass(const Volume::Frustum& frustum, vec3 cameraLocation, const Ref<RenderList::Pass>& pass) {

        auto& meshComponentPool = scene->entityManager.GetPool<MeshComponent>();
        auto shadow = pass->type == RenderList::RenderPassType::Shadow;

        pass->frustum = frustum;
        pass->gpuDraws.clear();

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

            auto addDraw = [&](Mesh::MeshSubData* subData, RenderList::GPUDrawType type) {
                pass->gpuDraws.push_back({ subData, mesh.Get().get(), mesh.GetID(), slotIdx, type });
            };

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

    void GPUScene::Cull(RenderList& renderList, vec3 cameraLocation, Graphics::CommandList* commandList) {

        auto slotCount = uint32_t(cullMeshes.size());

        std::vector<GPUCullView> views;
        std::vector<GPUDrawRequest> requests;
        const auto& passes = renderList.gpuCulledPasses;
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
        }

        EnsureCullBuffers(views.size(), requests.size());
        renderList.gpuCommandBuffer = commandBuffer;

        if (requests.empty())
            return;

        Graphics::Profiler::BeginQuery("GPU culling");

        auto viewBuffer = uploadBufferPool.Upload(views.data(), views.size() * sizeof(GPUCullView));
        auto requestBuffer = uploadBufferPool.Upload(requests.data(), requests.size() * sizeof(GPUDrawRequest));

        // The previous frame might still draw with the commands and indices
        commandList->MemoryBarrier(VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        uint32_t zero = 0;
        commandList->FillBuffer(counterBuffer, &zero);

        commandList->MemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        if (instanceCount > 0) {
            auto pipeline = PipelineManager::GetPipeline(cullPipelineConfig);
            commandList->BindPipeline(pipeline);

            BindCullBuffers(viewBuffer, commandList);

            auto constants = CullPushConstants {
                .cameraLocation = vec4(cameraLocation, 1.0f),
                .instanceCount = instanceCount,
                .viewCount = uint32_t(views.size())
            };
            commandList->PushConstants("constants", &constants);
            commandList->Dispatch((instanceCount + 63) / 64, 1, 1);

            commandList->MemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        }

        auto pipeline = PipelineManager::GetPipeline(drawsPipelineConfig);
        commandList->BindPipeline(pipeline);

        commandList->BindBuffer(meshBuffer, 3, 0);
        commandList->BindBuffer(viewBuffer, 3, 1);
        commandList->BindBuffer(counterBuffer, 3, 2);
        commandList->BindBuffer(requestBuffer, 3, 3);
        commandList->BindBuffer(commandBuffer, 3, 4);

        auto drawRequestCount = uint32_t(requests.size());
        commandList->PushConstants("constants", &drawRequestCount);
        commandList->Dispatch((drawRequestCount + 63) / 64, 1, 1);

        commandList->MemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);

        Graphics::Profiler::EndQuery();

    }

    void GPUScene::WriteRayTracingInstances(RayTracing::RayTracingWorld& rayTracingWorld, vec3 cameraLocation,
        Graphics::CommandList* commandList) {

        auto pendingInstances = rayTracingWorld.GetPendingInstances();
        if (!pendingInstances)
            return;

        Graphics::Profiler::BeginQuery("Ray tracing instances");

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
                .includeObjectHistory = pendingInstances->includeObjectHistory ? 1u : 0u
            };
            commandList->PushConstants("constants", &constants);
            commandList->Dispatch((instanceCount + 63) / 64, 1, 1);
        }

        Graphics::Profiler::EndQuery();

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

        auto bounds = slot.mesh->data.aabb.Transform(globalMatrix);
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
        totalCapacity = 0;
        impostorCapacity = 0;

        for (size_t slotIdx = 0; slotIdx < slots.size(); slotIdx++) {
            auto& slot = slots[slotIdx];

            slot.drawable = false;
            slot.skinned = false;
            slot.impostor = false;

            if (slotIdx > 0 && !slot.entities.empty() && slot.mesh.IsLoaded()) {
                slot.skinned = slot.mesh->data.IsSkinned();
                slot.drawable = !slot.skinned;
                slot.impostor = slot.drawable && slot.mesh->impostor.IsLoaded() && slot.mesh->impostor->isGenerated;
            }

            // The mesh might have been loaded after its instances were added
            if (slot.drawable && !slot.hasBounds) {
                for (auto entity : slot.entities)
                    GrowBounds(slot, transformComponentPool.Get(entity).globalMatrix);
            }

            GPUCullMesh cullMesh = {};
            if (slot.drawable) {
                const auto& mesh = slot.mesh;
                auto count = uint32_t(slot.entities.size());

                slot.regionOffset = totalCapacity;
                slot.impostorRegionOffset = impostorCapacity;
                totalCapacity += count;
                impostorCapacity += slot.impostor ? count : 0;

                cullMesh = GPUCullMesh {
                    .aabbMin = vec4(mesh->data.aabb.min, mesh->distanceCulling),
                    .aabbMax = vec4(mesh->data.aabb.max, mesh->shadowDistanceCulling),
                    .impostorDistance = mesh->impostorDistance,
                    .impostorShadowDistance = mesh->impostorShadowDistance,
                    .flags = MeshDrawable | (mesh->castShadow ? MeshCastShadow : 0u) | (slot.impostor ? MeshImpostor : 0u),
                    .regionOffset = slot.regionOffset,
                    .regionCapacity = count,
                    .impostorRegionOffset = slot.impostorRegionOffset
                };
            }

            cullMeshes[slotIdx] = cullMesh;
        }

    }

    void GPUScene::EnsureCullBuffers(size_t viewCount, size_t requestCount) {

        auto usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        viewCount = std::max(viewCount, size_t(1));
        auto slotCount = cullMeshes.size();
        EnsureBuffer(counterBuffer, sizeof(uint32_t) * drawTypeCount * viewCount * slotCount, usage);
        EnsureBuffer(indexBuffer, sizeof(uint32_t) * std::max(viewCount * totalCapacity, size_t(1)), usage);
        EnsureBuffer(impostorIndexBuffer, sizeof(uint32_t) * std::max(viewCount * impostorCapacity, size_t(1)), usage);
        EnsureBuffer(commandBuffer, RenderList::gpuDrawCommandSize * std::max(requestCount, size_t(1)),
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
