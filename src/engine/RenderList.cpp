#include "RenderList.h"

#include "graphics/GraphicsDevice.h"
#include "scene/components/TransformComponent.h"

#include <glm/gtx/norm.hpp>

namespace Atlas {

    RenderList::RenderList()  {



    }

    void RenderList::NewFrame() {

        auto lastSize = currentActorMatrices.size();
        currentActorMatrices.clear();
        if (lastSize) currentActorMatrices.reserve(lastSize);

        lastSize = lastActorMatrices.size();
        lastActorMatrices.clear();
        if (lastSize) lastActorMatrices.reserve(lastSize);

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

    void RenderList::NewShadowPass(Lighting::Light *light, uint32_t layer) {

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

    RenderList::Pass* RenderList::GetShadowPass(const Lighting::Light *light, const uint32_t layer) {

        for (auto& pass : passes) {
            if (pass.type == RenderPassType::Shadow &&
                pass.light == light && pass.layer == layer) return &pass;
        }

        return nullptr;

    }

    void RenderList::Add(Scene::Entity entity, Scene::Components::MeshComponent& meshComponent) {

        auto& pass = passes.back();
        auto& meshToActorMap = pass.meshToActorMap;

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

    void RenderList::Update(Camera* camera) {

        auto cameraLocation = camera->GetLocation();

        auto& pass = passes.back();
        auto type = pass.type;
        auto& meshToActorMap = pass.meshToActorMap;
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

        currentActorMatrices.reserve(maxActorCount);
        lastActorMatrices.reserve(maxActorCount);
        impostorMatrices.reserve(maxImpostorCount);

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

            instances.offset = currentActorMatrices.size();
            instances.impostorOffset = impostorMatrices.size();

            if (hasImpostor) {
                for (auto entity : entities) {
                    auto& transformComponent = entity.GetComponent<Scene::Components::TransformComponent>();
                    auto distance = glm::distance2(
                        vec3(transformComponent.globalMatrix[3]),
                        cameraLocation);

                    if (distance < sqdDistance) {
                        currentActorMatrices.push_back(glm::transpose(transformComponent.globalMatrix));
                        if (needsHistory) lastActorMatrices.push_back(glm::transpose(transformComponent.lastGlobalMatrix));
                    }
                    else {
                        impostorMatrices.push_back(glm::transpose(transformComponent.globalMatrix));
                    }
                    //impostorMatrices.push_back(actor->globalMatrix);
                }
            }
            else {
                for (auto entity : entities) {
                    auto& transformComponent = entity.GetComponent<Scene::Components::TransformComponent>();
                    currentActorMatrices.push_back(glm::transpose(transformComponent.globalMatrix));
                    if (needsHistory) {
                        lastActorMatrices.push_back(glm::transpose(transformComponent.lastGlobalMatrix));
                    }
                    else {
                        // For now push back anyways
                        lastActorMatrices.push_back(glm::transpose(transformComponent.globalMatrix));
                    }
                }
            }

            instances.count = currentActorMatrices.size() - instances.offset;
            instances.impostorCount = impostorMatrices.size() - instances.impostorOffset;
            meshToInstancesMap[meshId] = instances;

        }

    }

    void RenderList::FillBuffers() {

        auto device = Graphics::GraphicsDevice::DefaultDevice;

        if (!currentMatricesBuffer || currentMatricesBuffer->size < sizeof(mat3x4) * currentActorMatrices.size()) {
            auto newSize = currentMatricesBuffer != nullptr ? currentMatricesBuffer->size * 2 :
                           sizeof(mat3x4) * currentActorMatrices.size();
            newSize = std::max(std::max(newSize, size_t(1)), sizeof(mat3x4) * currentActorMatrices.size());
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
        if (currentActorMatrices.size() > 0)
            currentMatricesBuffer->SetData(currentActorMatrices.data(), 0, currentActorMatrices.size() * sizeof(mat3x4));
        if (lastActorMatrices.size() > 0)
            lastMatricesBuffer->SetData(lastActorMatrices.data(), 0, lastActorMatrices.size() * sizeof(mat3x4));
        if (impostorMatrices.size() > 0)
            impostorMatricesBuffer->SetData(impostorMatrices.data(), 0, impostorMatrices.size() * sizeof(mat3x4));

    }

}