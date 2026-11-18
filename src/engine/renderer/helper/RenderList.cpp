#include "RenderList.h"
#include "common/TransformOrientation.h"

#include "graphics/GraphicsDevice.h"
#include "scene/Scene.h"
#include "scene/components/TransformComponent.h"
#include "scene/components/SoftBodyComponent.h"

#include <glm/gtx/norm.hpp>

#include <algorithm>
#include <span>

namespace Atlas {

    RenderList::RenderList()  {



    }

    RenderList::~RenderList() {

        JobSystem::Wait(clearJob);

    }

    void RenderList::NewFrame(Scene::Scene* scene, const std::vector<ResourceHandle<Mesh::Mesh>>& meshes) {

        std::scoped_lock lock(mutex);
        this->scene = scene;

        processedMainPasses.clear();
        processedShadowPasses.clear();
        gpuCulledPasses.clear();

        JobSystem::WaitSpin(clearJob);
        wasCleared = false;

        meshIdToMeshMap.reserve(meshes.size());

        // Fill in missing meshes since they are cleared at the end of each frame
        for (const auto& mesh : meshes) {
            auto id = mesh.GetID();
            meshIdToMeshMap[id] = mesh;
        }

        // Like the GPU culling, which knows the focal length of the last frame
        lodFocalLength = scene->renderState.gpuScene.GetFocalLength();
        lodErrorScale = scene->meshLodErrorScale;
        forcedLod = scene->forcedMeshLod;
    }

    void RenderList::UpdateSoftBodyVertices(Scene::Scene* scene) {

        currentSoftBodyVertices.clear();
        lastSoftBodyVertices.clear();
        entityToSoftBodyVertices.clear();

        // Entities without vertices are drawn like the other instances of their mesh
        auto softBodySubset = scene->entityManager.GetSubset<SoftBodyComponent, MeshComponent>();
        for (auto entity : softBodySubset) {
            const auto& [softBodyComponent, meshComponent] = softBodySubset.Get(entity);
            const auto& mesh = meshComponent.mesh;
            if (!softBodyComponent.IsValid() || !mesh.IsLoaded() || mesh->data.IsSkinned() ||
                softBodyComponent.vertices.size() != size_t(mesh->data.GetVertexCount() - mesh->data.lodVertexCount))
                continue;

            entityToSoftBodyVertices[entity] = uint32_t(currentSoftBodyVertices.size());
            currentSoftBodyVertices.insert(currentSoftBodyVertices.end(), softBodyComponent.vertices.begin(),
                softBodyComponent.vertices.end());
            lastSoftBodyVertices.insert(lastSoftBodyVertices.end(), softBodyComponent.lastVertices.begin(),
                softBodyComponent.lastVertices.end());
        }

        currentSoftBodyVerticesBuffer = currentSoftBodyVertices.empty() ? nullptr :
            uploadPool.Upload(currentSoftBodyVertices.data(), currentSoftBodyVertices.size() * sizeof(vec4));
        lastSoftBodyVerticesBuffer = lastSoftBodyVertices.empty() ? nullptr :
            uploadPool.Upload(lastSoftBodyVertices.data(), lastSoftBodyVertices.size() * sizeof(vec4));

    }

    uint32_t RenderList::GetLod(const Mesh::Mesh& mesh, const mat4& globalMatrix, const Volume::AABB& bounds,
        vec3 cameraLocation) {

        return uint32_t(mesh.SelectLod(globalMatrix, bounds, cameraLocation, lodFocalLength, lodErrorScale, forcedLod));

    }

    Ref<RenderList::Pass> RenderList::NewMainPass() {

        std::scoped_lock lock(mutex);
        Pass pass{
            .type = RenderPassType::Main,
            .layer = 0,
            .scene = scene,
            .wasUsed = true,
        };

        passes.push_back(CreateRef(pass));
        return passes.back();

    }

    Ref<RenderList::Pass> RenderList::NewShadowPass(ECS::Entity lightEntity, uint32_t layer) {

        std::scoped_lock lock(mutex);
        Pass pass {
            .type = RenderPassType::Shadow,
            .lightEntity = Scene::Entity(lightEntity, &scene->entityManager),
            .layer = layer,
            .scene = scene,
            .wasUsed = true,
        };

        passes.push_back(CreateRef(pass));
        return passes.back();

    }

    Ref<RenderList::Pass> RenderList::GetMainPass() {

        std::scoped_lock lock(mutex);

        for (auto& pass : passes) {
            if (pass->type == RenderPassType::Main) {
                pass->wasUsed = true;
                return pass;
            }
        }

        return nullptr;

    }

    Ref<RenderList::Pass> RenderList::GetShadowPass(ECS::Entity lightEntity, const uint32_t layer) {

        std::scoped_lock lock(mutex);
        for (auto& pass : passes) {
            if (pass->type == RenderPassType::Shadow &&
                pass->lightEntity == lightEntity && pass->layer == layer) {
                // Update entity in case it came from a different scene
                pass->lightEntity = Scene::Entity(lightEntity, &scene->entityManager);
                pass->wasUsed = true;
                return pass;
            }
        }

        return nullptr;

    }

    void RenderList::FinishPass(const Ref<Pass>& pass, RenderPassType type) {

        std::scoped_lock lock(mutex);

        if (type == RenderPassType::Main) {
            processedMainPasses.push_back(pass);
        }
        else {
            processedShadowPasses.push_back(pass);
        }

    }

    void RenderList::AddGPUCulledPass(const Ref<Pass>& pass) {

        std::scoped_lock lock(mutex);

        pass->gpuCulled = true;
        gpuCulledPasses.push_back(pass);

    }

    void RenderList::DrawGPUCulled(Graphics::CommandList* commandList, const GPUDraw& draw) const {

        auto offset = draw.command * gpuDrawCommandSize;
        if (draw.type != GPUDrawType::Impostor)
            commandList->DrawIndexedIndirect(gpuCommandBuffer, offset, draw.lodCount, gpuDrawCommandSize);
        else
            commandList->DrawIndirect(gpuCommandBuffer, offset, 1, gpuDrawCommandSize);

    }

    Ref<RenderList::Pass> RenderList::PopPassFromQueue(RenderPassType type) {

        std::scoped_lock lock(mutex);
        auto& processedPasses = type == RenderPassType::Main ? processedMainPasses : processedShadowPasses;

        if (processedPasses.empty())
            return nullptr;

        auto pass = processedPasses.front();
        if (pass->type != type)
            return nullptr;

        processedPasses.pop_front();
        return pass;

    }

    void RenderList::Clear() {

        // We can reset the scene now and delete the reference
        wasCleared = true;
        scene = nullptr;
        gpuCulledPasses.clear();

        JobSystem::Execute(clearJob, 
            [&](JobData&) {
            std::erase_if(passes, [](const auto& item) { return item->wasUsed != true; });
            meshIdToMeshMap.clear();
            for (auto& pass : passes)
                pass->Reset();
            });

    }

    void RenderList::Pass::NewFrame(Scene::Scene* scene, const std::vector<ResourceHandle<Mesh::Mesh>>& meshes,
        const std::unordered_map<size_t, ResourceHandle<Mesh::Mesh>>& meshIdToMeshMap) {

        this->scene = scene;

        auto lastSize = currentEntityMatrices.size();
        currentEntityMatrices.clear();
        if (lastSize) currentEntityMatrices.reserve(lastSize);

        lastSize = lastEntityMatrices.size();
        lastEntityMatrices.clear();
        if (lastSize) lastEntityMatrices.reserve(lastSize);

        lastSize = impostorMatrices.size();
        impostorMatrices.clear();
        if (lastSize) impostorMatrices.reserve(lastSize);

        lastSize = currentJointMatrices.size();
        currentJointMatrices.clear();
        if (lastSize) currentJointMatrices.reserve(lastSize);

        lastSize = lastJointMatrices.size();
        lastJointMatrices.clear();
        if (lastSize) lastJointMatrices.reserve(lastSize);

        softBodyVertexOffsets.clear();

        lastSize = instanceIndices.size();
        instanceIndices.clear();
        if (lastSize) instanceIndices.reserve(lastSize);

        lastSize = impostorInstanceIndices.size();
        impostorInstanceIndices.clear();
        if (lastSize) impostorInstanceIndices.reserve(lastSize);

        gpuCulled = false;
        gpuDraws.clear();
        gpuLateDraws.clear();

        std::erase_if(meshToEntityMap, [&](const auto& item) { return !meshIdToMeshMap.contains(item.first); });
        std::erase_if(meshToInstancesMap, [&](const auto& item) { return !meshIdToMeshMap.contains(item.first); });
        meshToSoftBodyInstancesMap.clear();

    }

    void RenderList::Pass::Add(const ECS::Entity& entity, const MeshComponent& meshComponent) {

        auto id = meshComponent.mesh.GetID();

        auto item = meshToEntityMap.find(id);
        if (item != meshToEntityMap.end()) {
            item->second.Add(entity);
        }
        else {
            EntityBatch batch;
            batch.Add(entity);

            meshToEntityMap[id] = batch;
        }

    }

    void RenderList::Pass::Add(int32_t threadIdx, const ECS::Entity& entity, const MeshComponent& meshComponent) {

        auto& context = contexts[threadIdx];

        context.entities.push_back({ entity, meshComponent.mesh.GetID() });

    }

    void RenderList::Pass::Finalize() {

        for (const auto& context : contexts) {
            for (const auto& [entity, meshId] : context.entities) {
                auto item = meshToEntityMap.find(meshId);
                if (item != meshToEntityMap.end()) {
                    item->second.Add(entity);
                }
                else {
                    EntityBatch batch;
                    batch.Add(entity);

                    meshToEntityMap[meshId] = batch;
                }
            }
        }

    }

    void RenderList::Pass::Update(vec3 cameraLocation, RenderList& renderList) {

        auto& transformPool = scene->entityManager.GetPool<TransformComponent>();
        auto& animationPool = scene->entityManager.GetPool<AnimationComponent>();
        auto& meshPool = scene->entityManager.GetPool<MeshComponent>();
        const auto& meshIdToMeshMap = renderList.meshIdToMeshMap;
        for (auto& [meshId, batch] : meshToEntityMap) {
            auto item = meshIdToMeshMap.find(meshId);
            // This happens when meshes are loaded async
            if (item == meshIdToMeshMap.end())
                continue;
            auto mesh = item->second;
            if (!batch.count) continue;
            if (!mesh->castShadow && type == RenderPassType::Shadow) continue;

            auto isSkinned = mesh->data.IsSkinned();
            auto jointCount = size_t(mesh->data.skeleton.GetJointCount());
            auto needsHistory = mesh->mobility != Mesh::MeshMobility::Stationary
                && type != RenderPassType::Shadow;

            auto typeDistance = type == RenderPassType::Shadow ?
                mesh->impostorShadowDistance : mesh->impostorDistance;
            auto sqdDistance = typeDistance * typeDistance;

            // Entities with simulated soft body vertices are drawn apart from the other instances of the mesh
            const auto& softBodyVertices = renderList.entityToSoftBodyVertices;
            auto batchEnd = batch.entities.begin() + batch.count;
            auto softBodyBegin = isSkinned || softBodyVertices.empty() ? batchEnd :
                std::stable_partition(batch.entities.begin(), batchEnd, [&](ECS::Entity entity) {
                    return !softBodyVertices.contains(entity);
                    });

            auto addInstances = [&](std::span<ECS::Entity> entities, bool isSoftBody) {
                auto hasImpostor = mesh->impostor.IsLoaded() && mesh->impostor->isGenerated && mesh->impostor->IsResident() &&
                    !isSkinned && !isSoftBody;

                // Skinned meshes and soft bodies need their joint matrices or vertices next to the matrices of the instances
                auto useIndices = !isSkinned && !isSoftBody;
                auto instanceCount = [&]() { return useIndices ? instanceIndices.size() : currentEntityMatrices.size(); };

                MeshInstances instances{};
                instances.offset = instanceCount();
                instances.impostorOffset = useIndices ? impostorInstanceIndices.size() : impostorMatrices.size();
                instances.jointOffset = isSoftBody ? softBodyVertexOffsets.size() : currentJointMatrices.size();

                auto reversedWindingBegin = Common::PartitionByWinding(entities.begin(), entities.end(),
                    [&](ECS::Entity entity) -> const mat4& {
                        return transformPool.Get(entity).globalMatrix;
                    });
                const auto defaultWindingEntityCount = size_t(reversedWindingBegin - entities.begin());

                // All passes use the level of detail of the main camera, like the GPU culling
                auto selectLods = mesh->data.GetLodCount() > 1 && !isSoftBody;
                if (selectLods) {
                    entityLods.resize(entities.size());
                    for (size_t i = 0; i < entities.size(); i++) {
                        auto entity = entities[i];
                        auto lod = renderList.GetLod(*mesh.Get(), transformPool.Get(entity).globalMatrix,
                            meshPool.Get(entity).aabb, cameraLocation);
                        entityLods[i] = { lod, entity };
                    }

                    auto byLod = [](const auto& entity0, const auto& entity1) { return entity0.first < entity1.first; };
                    std::stable_sort(entityLods.begin(), entityLods.begin() + defaultWindingEntityCount, byLod);
                    std::stable_sort(entityLods.begin() + defaultWindingEntityCount, entityLods.end(), byLod);
                    for (size_t i = 0; i < entities.size(); i++)
                        entities[i] = entityLods[i].second;
                }

                for (size_t i = 0; i < entities.size(); i++) {
                    if (i == defaultWindingEntityCount)
                        instances.reversedWindingOffset = instanceCount();
                    auto& transformComponent = transformPool.Get(entities[i]);
                    const auto distance = hasImpostor ? glm::distance2(
                        vec3(transformComponent.globalMatrix[3]), cameraLocation) : 0.0f;
                    if (hasImpostor && distance >= sqdDistance) {
                        if (useIndices)
                            impostorInstanceIndices.push_back(ECS::EntityToIdx(entities[i]));
                        else
                            impostorMatrices.push_back(glm::transpose(transformComponent.globalMatrix));
                        continue;
                    }
                    auto lod = selectLods ? entityLods[i].first : 0u;
                    if (i < defaultWindingEntityCount)
                        instances.lodCounts[lod]++;
                    else
                        instances.reversedWindingLodCounts[lod]++;

                    if (useIndices) {
                        instanceIndices.push_back(ECS::EntityToIdx(entities[i]));
                        continue;
                    }
                    currentEntityMatrices.push_back(glm::transpose(transformComponent.globalMatrix));
                    if (needsHistory) {
                        lastEntityMatrices.push_back(glm::transpose(transformComponent.lastGlobalMatrix));
                    }
                    else if (type != RenderPassType::Shadow) {
                        lastEntityMatrices.push_back(currentEntityMatrices.back());
                    }

                    if (isSoftBody) {
                        softBodyVertexOffsets.push_back(softBodyVertices.at(entities[i]));
                        continue;
                    }

                    if (!isSkinned)
                        continue;

                    // Without an animation the mesh is rendered in its rest pose
                    auto animationComponent = animationPool.TryGet(entities[i]);
                    if (animationComponent && animationComponent->jointMatrices.size() == jointCount) {
                        currentJointMatrices.insert(currentJointMatrices.end(), animationComponent->jointMatrices.begin(),
                            animationComponent->jointMatrices.end());
                        if (type != RenderPassType::Shadow)
                            lastJointMatrices.insert(lastJointMatrices.end(), animationComponent->lastJointMatrices.begin(),
                                animationComponent->lastJointMatrices.end());
                    }
                    else {
                        const auto& restJointMatrices = mesh->data.skeleton.restJointMatrices;
                        currentJointMatrices.insert(currentJointMatrices.end(), restJointMatrices.begin(), restJointMatrices.end());
                        if (type != RenderPassType::Shadow)
                            lastJointMatrices.insert(lastJointMatrices.end(), restJointMatrices.begin(), restJointMatrices.end());
                    }
                }
                if (defaultWindingEntityCount == entities.size())
                    instances.reversedWindingOffset = instanceCount();
                instances.reversedWindingCount = instanceCount() - instances.reversedWindingOffset;

                instances.count = instanceCount() - instances.offset;
                instances.impostorCount = (useIndices ? impostorInstanceIndices.size() : impostorMatrices.size())
                    - instances.impostorOffset;
                return instances;
            };

            meshToInstancesMap[meshId] = addInstances(std::span(batch.entities.begin(), softBodyBegin), false);
            if (softBodyBegin != batchEnd)
                meshToSoftBodyInstancesMap[meshId] = addInstances(std::span(softBodyBegin, batchEnd), true);

        }

    }

    void RenderList::Pass::FillBuffers() {

        auto device = Graphics::GraphicsDevice::DefaultDevice;

        if (!currentMatricesBuffer || currentMatricesBuffer->size < sizeof(mat3x4) * currentEntityMatrices.size()) {
            auto newSize = currentMatricesBuffer != nullptr ? currentMatricesBuffer->size * 2 :
                           sizeof(mat3x4) * currentEntityMatrices.size();
            newSize = std::max(std::max(newSize, size_t(1)), sizeof(mat3x4) * currentEntityMatrices.size());
            auto bufferDesc = Graphics::BufferDesc {
                .usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .domain = Graphics::BufferDomain::Host,
                .size = newSize
            };
            if (newSize > 0) currentMatricesBuffer = device->CreateMultiBuffer(bufferDesc);
            // Only main pass needs the history
            if (newSize > 0 && type != RenderPassType::Shadow) lastMatricesBuffer = device->CreateMultiBuffer(bufferDesc);
        }

        if (!impostorMatricesBuffer || impostorMatricesBuffer->size < sizeof(mat3x4) * impostorMatrices.size()) {
            auto newSize = impostorMatricesBuffer != nullptr ? impostorMatricesBuffer->size * 2 :
                           sizeof(mat3x4) * impostorMatrices.size();
            newSize = std::max(std::max(newSize, size_t(1)), sizeof(mat3x4) * impostorMatrices.size());
            auto bufferDesc = Graphics::BufferDesc {
                .usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .domain = Graphics::BufferDomain::Host,
                .size = newSize
            };
            if (newSize > 0) impostorMatricesBuffer = device->CreateMultiBuffer(bufferDesc);
        }

        if (!currentJointMatricesBuffer || currentJointMatricesBuffer->size < sizeof(mat3x4) * currentJointMatrices.size()) {
            auto newSize = currentJointMatricesBuffer != nullptr ? currentJointMatricesBuffer->size * 2 :
                           sizeof(mat3x4) * currentJointMatrices.size();
            newSize = std::max(std::max(newSize, size_t(1)), sizeof(mat3x4) * currentJointMatrices.size());
            auto bufferDesc = Graphics::BufferDesc {
                .usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .domain = Graphics::BufferDomain::Host,
                .size = newSize
            };
            if (newSize > 0) currentJointMatricesBuffer = device->CreateMultiBuffer(bufferDesc);
            // Only main pass needs the history
            if (newSize > 0 && type != RenderPassType::Shadow) lastJointMatricesBuffer = device->CreateMultiBuffer(bufferDesc);
        }

        softBodyVertexOffsetsBuffer = softBodyVertexOffsets.empty() ? nullptr :
            uploadPool->Upload(softBodyVertexOffsets.data(), softBodyVertexOffsets.size() * sizeof(uint32_t));

        auto ensureIndicesBuffer = [&](Ref<Graphics::MultiBuffer>& buffer, size_t count) {
            if (buffer && buffer->size >= sizeof(uint32_t) * count)
                return;
            auto newSize = buffer != nullptr ? buffer->size * 2 : size_t(0);
            newSize = std::max(newSize, sizeof(uint32_t) * std::max(count, size_t(1)));
            auto bufferDesc = Graphics::BufferDesc {
                .usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .domain = Graphics::BufferDomain::Host,
                .size = newSize
            };
            buffer = device->CreateMultiBuffer(bufferDesc);
        };

        ensureIndicesBuffer(instanceIndicesBuffer, instanceIndices.size());
        ensureIndicesBuffer(impostorInstanceIndicesBuffer, impostorInstanceIndices.size());

        if (instanceIndices.size() > 0)
            instanceIndicesBuffer->SetData(instanceIndices.data(), 0, instanceIndices.size() * sizeof(uint32_t));
        if (impostorInstanceIndices.size() > 0)
            impostorInstanceIndicesBuffer->SetData(impostorInstanceIndices.data(), 0,
                impostorInstanceIndices.size() * sizeof(uint32_t));

        // Probably better to use a device local buffer since we upload once per frame
        if (currentEntityMatrices.size() > 0)
            currentMatricesBuffer->SetData(currentEntityMatrices.data(), 0, currentEntityMatrices.size() * sizeof(mat3x4));
        if (lastEntityMatrices.size() > 0)
            lastMatricesBuffer->SetData(lastEntityMatrices.data(), 0, lastEntityMatrices.size() * sizeof(mat3x4));
        if (impostorMatrices.size() > 0)
            impostorMatricesBuffer->SetData(impostorMatrices.data(), 0, impostorMatrices.size() * sizeof(mat3x4));
        if (currentJointMatrices.size() > 0)
            currentJointMatricesBuffer->SetData(currentJointMatrices.data(), 0, currentJointMatrices.size() * sizeof(mat3x4));
        if (lastJointMatrices.size() > 0)
            lastJointMatricesBuffer->SetData(lastJointMatrices.data(), 0, lastJointMatrices.size() * sizeof(mat3x4));

    }

    void RenderList::Pass::Reset() {

        std::erase_if(meshToEntityMap, [](const auto& item) { return item.second.count == 0; });
        for (auto& [id, batch] : meshToEntityMap) {
            batch.entities.resize(batch.count);
            batch.count = 0;
        }

        // Need to clear this to free the references
        meshToInstancesMap.clear();
        meshToSoftBodyInstancesMap.clear();

        for (auto& context : contexts)
            context.entities.clear();

        gpuCulled = false;
        gpuDraws.clear();
        gpuLateDraws.clear();

        wasUsed = false;
        scene = nullptr;

    }

}
