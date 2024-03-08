#include "RenderList.h"

#include "graphics/GraphicsDevice.h"
#include "scene/Scene.h"
#include "scene/components/TransformComponent.h"

#include <glm/gtx/norm.hpp>

namespace Atlas {

    RenderList::RenderList()  {



    }

    void RenderList::NewFrame(Ref<Scene::Scene> scene) {

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

        // Fill in missing meshes since they are cleared at the end of each frame
        auto meshes = scene->GetMeshes();
        for (auto& pass : passes) {
            for (const auto& mesh : meshes) {
                auto id = mesh.GetID();
                pass->meshIdToMeshMap[id] = mesh;
            }

            std::erase_if(pass->meshToEntityMap, [pass](auto& item) { return !pass->meshIdToMeshMap.contains(item.first); });
            std::erase_if(pass->meshToInstancesMap, [pass](auto& item) { return !pass->meshIdToMeshMap.contains(item.first); });
        }
    }

    Ref<RenderList::Pass> RenderList::NewMainPass() {

        Pass pass {
            .type = RenderPassType::Main,
            .lightEntity = ECS::EntityConfig::InvalidEntity,
            .layer = 0
        };

        passes.push_back(CreateRef(pass));
        return passes.back();

    }

    Ref<RenderList::Pass> RenderList::NewShadowPass(ECS::Entity lightEntity, uint32_t layer) {

        Pass pass{
            .type = RenderPassType::Shadow,
            .lightEntity = lightEntity,
            .layer = layer,
            .wasUsed = true
        };

        passes.push_back(CreateRef(pass));
        return passes.back();

    }

    Ref<RenderList::Pass> RenderList::GetMainPass() {

        for (auto& pass : passes) {
            if (pass->type == RenderPassType::Main) return pass;
        }

        return nullptr;

    }

    Ref<RenderList::Pass> RenderList::GetShadowPass(ECS::Entity lightEntity, const uint32_t layer) {

        for (auto& pass : passes) {
            if (pass->type == RenderPassType::Shadow &&
                pass->lightEntity == lightEntity && pass->layer == layer) return pass;
        }

        return nullptr;

    }

    void RenderList::Add(const Ref<Pass>& pass, const ECS::Entity& entity, const MeshComponent& meshComponent) {

        auto& meshToActorMap = pass->meshToEntityMap;

        if (!meshComponent.mesh.IsLoaded())
            return;

        auto id = meshComponent.mesh.GetID();

        auto item = meshToActorMap.find(id);
        if (item != meshToActorMap.end()) {
            item->second.Add(entity);
        }
        else {
            auto& meshIdToMeshMap = pass->meshIdToMeshMap;

            EntityBatch batch;
            batch.Add(entity);

            meshToActorMap[id] = batch;
            meshIdToMeshMap[id] = meshComponent.mesh;
        }

    }

    void RenderList::Update(const Ref<Pass>& pass, vec3 cameraLocation) {

        auto type = pass->type;
        auto& meshToEntityMap = pass->meshToEntityMap;
        auto& meshToInstancesMap = pass->meshToInstancesMap;
        auto& meshIdToMeshMap = pass->meshIdToMeshMap;

        size_t maxActorCount = 0;
        size_t maxImpostorCount = 0;

        for (auto& [meshId, batch] : meshToEntityMap) {
            auto mesh = meshIdToMeshMap[meshId];
            if (!mesh->castShadow && type == RenderPassType::Shadow)
                continue;

            auto hasImpostor = mesh->impostor != nullptr;
            maxActorCount += batch.count;
            maxImpostorCount += hasImpostor ? batch.count : 0;
        }

        for (auto& [meshId, batch] : meshToEntityMap) {
            auto mesh = meshIdToMeshMap[meshId];
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
                        else {
                            // For now push back anyways (since indices of matrices per entity need to match)
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
                    else {
                        // For now push back anyways
                        lastEntityMatrices.push_back(currentEntityMatrices.back());
                    }
                }
            }

            instances.count = currentEntityMatrices.size() - instances.offset;
            instances.impostorCount = impostorMatrices.size() - instances.impostorOffset;
            meshToInstancesMap[meshId] = instances;

        }

    }

    void RenderList::FillBuffers() {

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
            if (newSize > 0) lastMatricesBuffer = device->CreateMultiBuffer(bufferDesc);
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

    void RenderList::Clear() {

        // We can reset the scene now and delete the reference
        scene = nullptr;

        std::erase_if(passes, [](auto& item) { return item->wasUsed != true; });
        for (auto& pass : passes)
            pass->Reset();

    }

}