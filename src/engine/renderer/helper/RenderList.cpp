#include "RenderList.h"

#include "graphics/GraphicsDevice.h"
#include "scene/Scene.h"
#include "scene/components/TransformComponent.h"

#include <glm/gtx/norm.hpp>

namespace Atlas {

    RenderList::RenderList()  {



    }

    RenderList::~RenderList() {

        JobSystem::Wait(clearJob);

    }

    void RenderList::NewFrame(Scene::Scene* scene) {

        std::scoped_lock lock(mutex);
        this->scene = scene;

        doneProcessingShadows = false;
        processedPasses.clear();

        JobSystem::Wait(clearJob);

        auto meshes = scene->GetMeshes();
        meshIdToMeshMap.reserve(meshes.size());

        // Fill in missing meshes since they are cleared at the end of each frame
        for (const auto& mesh : meshes) {
            auto id = mesh.GetID();
            meshIdToMeshMap[id] = mesh;
        }

    }

    Ref<RenderList::Pass> RenderList::NewMainPass() {

        std::scoped_lock lock(mutex);
        Pass pass{
            .type = RenderPassType::Main,
            .layer = 0,
            .scene = scene,
            .wasUsed = true,
        };

        doneProcessingShadows = true;

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
        doneProcessingShadows = true;

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

    void RenderList::FinishPass(const Ref<Pass>& pass) {

        std::scoped_lock lock(mutex);
        processedPasses.push_back(pass);

    }

    Ref<RenderList::Pass> RenderList::PopPassFromQueue(RenderPassType type) {

        std::scoped_lock lock(mutex);
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
        scene = nullptr;

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

        std::erase_if(meshToEntityMap, [&](const auto& item) { return !meshIdToMeshMap.contains(item.first); });
        std::erase_if(meshToInstancesMap, [&](const auto& item) { return !meshIdToMeshMap.contains(item.first); });

    }

    void RenderList::Pass::Add(const ECS::Entity& entity, const MeshComponent& meshComponent) {

        if (!meshComponent.mesh.IsLoaded())
            return;

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

    void RenderList::Pass::Update(vec3 cameraLocation, const std::unordered_map<size_t, ResourceHandle<Mesh::Mesh>>& meshIdToMeshMap) {

        size_t maxActorCount = 0;
        size_t maxImpostorCount = 0;

        for (auto& [meshId, batch] : meshToEntityMap) {
            auto item = meshIdToMeshMap.find(meshId);
            // This happens when meshes are loaded async
            if (item == meshIdToMeshMap.end())
                continue;
            auto mesh = item->second;
            if (!mesh->castShadow && type == RenderPassType::Shadow)
                continue;

            auto hasImpostor = mesh->impostor != nullptr;
            maxActorCount += batch.count;
            maxImpostorCount += hasImpostor ? batch.count : 0;
        }

        for (auto& [meshId, batch] : meshToEntityMap) {
            auto item = meshIdToMeshMap.find(meshId);
            // This happens when meshes are loaded async
            if (item == meshIdToMeshMap.end())
                continue;
            auto mesh = item->second;
            if (!batch.count) continue;
            if (!mesh->castShadow && type == RenderPassType::Shadow) continue;

            auto hasImpostor = mesh->impostor != nullptr;
            auto needsHistory = mesh->mobility != Mesh::MeshMobility::Stationary
                && type != RenderPassType::Shadow;

            auto typeDistance = type == RenderPassType::Shadow ?
                mesh->impostorShadowDistance : mesh->impostorDistance;
            auto sqdDistance = typeDistance * typeDistance;

            MeshInstances instances;

            instances.offset = currentEntityMatrices.size();
            instances.impostorOffset = impostorMatrices.size();

            if (hasImpostor) {
                for (size_t i = 0; i < batch.count; i++) {
                    auto& ecsEntity = batch.entities[i];
                    auto entity = Scene::Entity(ecsEntity, &scene->entityManager);
                    auto& transformComponent = entity.GetComponent<TransformComponent>();
                    auto distance = glm::distance2(
                        vec3(transformComponent.globalMatrix[3]),
                        cameraLocation);

                    if (distance < sqdDistance) {
                        currentEntityMatrices.push_back(glm::transpose(transformComponent.globalMatrix));
                        if (needsHistory) {
                            lastEntityMatrices.push_back(glm::transpose(transformComponent.lastGlobalMatrix));
                        }
                        else if (type != RenderPassType::Shadow) {
                            // For now push back anyways for main pass (since indices of matrices per entity need to match)
                            lastEntityMatrices.push_back(currentEntityMatrices.back());
                        }
                    }
                    else {
                        impostorMatrices.push_back(glm::transpose(transformComponent.globalMatrix));
                    }
                }
            }
            else {
                for (size_t i = 0; i < batch.count; i++) {
                    auto& ecsEntity = batch.entities[i];
                    auto entity = Scene::Entity(ecsEntity, &scene->entityManager);
                    auto& transformComponent = entity.GetComponent<TransformComponent>();
                    currentEntityMatrices.push_back(glm::transpose(transformComponent.globalMatrix));
                    if (needsHistory) {
                        lastEntityMatrices.push_back(glm::transpose(transformComponent.lastGlobalMatrix));
                    }
                    else if (type != RenderPassType::Shadow) {
                        // For now push back anyways for main pass (since indices of matrices per entity need to match)
                        lastEntityMatrices.push_back(currentEntityMatrices.back());
                    }
                }
            }

            instances.count = currentEntityMatrices.size() - instances.offset;
            instances.impostorCount = impostorMatrices.size() - instances.impostorOffset;
            meshToInstancesMap[meshId] = instances;

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

        // Probably better to use a device local buffer since we upload once per frame
        if (currentEntityMatrices.size() > 0)
            currentMatricesBuffer->SetData(currentEntityMatrices.data(), 0, currentEntityMatrices.size() * sizeof(mat3x4));
        if (lastEntityMatrices.size() > 0)
            lastMatricesBuffer->SetData(lastEntityMatrices.data(), 0, lastEntityMatrices.size() * sizeof(mat3x4));
        if (impostorMatrices.size() > 0)
            impostorMatricesBuffer->SetData(impostorMatrices.data(), 0, impostorMatrices.size() * sizeof(mat3x4));

    }

    void RenderList::Pass::Reset() {

        std::erase_if(meshToEntityMap, [](const auto& item) { return item.second.count == 0; });
        for (auto& [id, batch] : meshToEntityMap) {
            batch.entities.resize(batch.count);
            batch.count = 0;
        }

        // Need to clear this to free the references
        meshToInstancesMap.clear();

        wasUsed = false;
        scene = nullptr;

    }

}
