#pragma once

#include "System.h"
#include "scene/Entity.h"
#include "scene/components/LightComponent.h"
#include "scene/components/MeshComponent.h"

#include "graphics/Buffer.h"

#include <unordered_map>
#include <vector>

namespace Atlas {

    namespace Scene {
        class Scene;
    }

    class RenderList {

    public:
        struct MeshInstances {
            size_t offset;
            size_t count;

            size_t impostorOffset;
            size_t impostorCount;
        };

        enum class RenderPassType {
            Main = 0,
            Shadow = 1
        };

        struct EntityBatch {
            size_t count = 0;
            std::vector<ECS::Entity> entities;

            inline void Add(ECS::Entity entity) {
                if (count < entities.size())
                    entities[count] = entity;
                else
                    entities.push_back(entity);
                count++;
            }
        };

        struct Pass {
            RenderPassType type;

            ECS::Entity lightEntity;
            uint32_t layer;

            std::unordered_map<size_t, EntityBatch> meshToEntityMap;
            std::unordered_map<size_t, MeshInstances> meshToInstancesMap;
            std::unordered_map<size_t, ResourceHandle<Mesh::Mesh>> meshIdToMeshMap;

            bool wasUsed = false;

            void Reset() { 
                
                std::erase_if(meshToEntityMap, [](auto& item) { return item.second.count == 0; });
                for (auto& [id, batch] : meshToEntityMap) {
                    batch.entities.resize(batch.count);
                    batch.count = 0;
                }

                // Need to clear this to free the references
                meshIdToMeshMap.clear();
                meshToInstancesMap.clear();

                wasUsed = false;

            }
        };

        RenderList();

        void NewFrame(Ref<Scene::Scene> scene);

        Ref<Pass> NewMainPass();

        Ref<Pass> NewShadowPass(const ECS::Entity lightEntity, uint32_t layer);

        Ref<Pass> GetMainPass();

        Ref<Pass> GetShadowPass(const ECS::Entity lightEntity, const uint32_t layer);

        void Add(const Ref<Pass>& pass, const ECS::Entity& entity, const MeshComponent& meshComponent);

        void Update(const Ref<Pass>& pass, vec3 cameraLocation);

        void FillBuffers();

        void Clear();

        Ref<Scene::Scene> scene = nullptr;

        std::vector<mat3x4> currentEntityMatrices;
        std::vector<mat3x4> lastEntityMatrices;
        std::vector<mat3x4> impostorMatrices;

        Ref<Graphics::MultiBuffer> currentMatricesBuffer;
        Ref<Graphics::MultiBuffer> lastMatricesBuffer;
        Ref<Graphics::MultiBuffer> impostorMatricesBuffer;

        std::vector<Ref<Pass>> passes;

    };

}