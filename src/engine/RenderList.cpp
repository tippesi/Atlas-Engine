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

        passes.clear();

    }

    void RenderList::NewMainPass() {

        Pass pass {
            .type = RenderPassType::Main,
            .light = nullptr,
            .layer = 0
        };

        passes.push_back(pass);

    }

    void RenderList::NewShadowPass(const LightComponent* light, uint32_t layer) {

        Pass pass {
            .type = RenderPassType::Shadow,
            .light = light,
            .layer = layer
        };

        passes.push_back(pass);

    }


    RenderList::Pass* RenderList::GetMainPass() {

        for (auto& pass : passes) {
            if (pass.type == RenderPassType::Main) return &pass;
        }

        return nullptr;

    }

    RenderList::Pass* RenderList::GetShadowPass(const LightComponent* light, const uint32_t layer) {

        for (auto& pass : passes) {
            if (pass.type == RenderPassType::Shadow &&
                pass.light == light && pass.layer == layer) return &pass;
        }

        return nullptr;

    }

    void RenderList::Add(const ECS::Entity& entity, const MeshComponent& meshComponent) {

        auto& pass = passes.back();
        auto& meshToActorMap = pass.meshToEntityMap;

        if (!meshComponent.mesh.IsLoaded())
            return;

        auto id = meshComponent.mesh.GetID();

        if (!meshToActorMap.contains(id)) {
            auto& meshIdToMeshMap = pass.meshIdToMeshMap;

            meshToActorMap[id] = { entity };
            meshIdToMeshMap[id] = meshComponent.mesh;
        }
        else {
            meshToActorMap[id].push_back(entity);
        }

    }

    void RenderList::Update(vec3 cameraLocation) {

        auto& pass = passes.back();
        auto type = pass.type;
        auto& meshToActorMap = pass.meshToEntityMap;
        auto& meshToInstancesMap = pass.meshToInstancesMap;
        auto& meshIdToMeshMap = pass.meshIdToMeshMap;

        size_t maxActorCount = 0;
        size_t maxImpostorCount = 0;

        for (auto& [meshId, actors] : meshToActorMap) {
            auto mesh = meshIdToMeshMap[meshId];
            if (!mesh->castShadow && type == RenderPassType::Shadow)
                continue;

            auto hasImpostor = mesh->impostor != nullptr;
            maxActorCount += actors.size();
            maxImpostorCount += hasImpostor ? actors.size() : 0;
        }

        for (auto& [meshId, entities] : meshToActorMap) {
            auto mesh = meshIdToMeshMap[meshId];
            if (!entities.size()) continue;
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
                for (auto ecsEntity : entities) {
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
                            lastEntityMatrices.push_back(glm::transpose(transformComponent.globalMatrix));
                        }
                    }
                    else {
                        impostorMatrices.push_back(glm::transpose(transformComponent.globalMatrix));
                    }
                }
            }
            else {
                for (auto ecsEntity : entities) {
                    auto entity = Scene::Entity(ecsEntity, &scene->entityManager);
                    auto& transformComponent = entity.GetComponent<TransformComponent>();
                    currentEntityMatrices.push_back(glm::transpose(transformComponent.globalMatrix));
                    if (needsHistory) {
                        lastEntityMatrices.push_back(glm::transpose(transformComponent.lastGlobalMatrix));
                    }
                    else {
                        // For now push back anyways
                        lastEntityMatrices.push_back(glm::transpose(transformComponent.globalMatrix));
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

}