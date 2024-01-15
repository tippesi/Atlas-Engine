#include "Scene.h"
#include "Entity.h"
#include "components/Components.h"

namespace Atlas {

    namespace NewScene {

        using namespace Components;

        Entity Scene::CreateEntity() {

            return Entity(entityManager.Create(), &entityManager);

        }

        void Scene::DestroyEntity(Entity entity) {

            entityManager.GetIfContains<HierarchyComponent>(entity);

            entityManager.Destroy(entity);

        }

        void Scene::Update(float deltaTime) {

            auto hierarchySubset = entityManager.GetSubset<HierarchyComponent, TransformComponent>();
            for (auto entity : hierarchySubset) {
                auto& hierarchyComponent = entityManager.Get<HierarchyComponent>(entity);
                auto& transformComponent = entityManager.Get<TransformComponent>(entity);

                if (hierarchyComponent.root) {
                    hierarchyComponent.Update(transformComponent, false);
                }
            }

            auto meshSubset = entityManager.GetSubset<MeshComponent, TransformComponent>();
            for (auto entity : meshSubset) {
                auto& meshComponent = entityManager.Get<MeshComponent>(entity);

                if (!meshComponent.mesh.IsLoaded())
                    continue;

                auto& transformComponent = entityManager.Get<TransformComponent>(entity);
                if (!transformComponent.changed)
                    continue;

                if (meshComponent.inserted)
                    SpacePartitioning::RemoveRenderableEntity(Entity(entity, &entityManager), transformComponent);

                transformComponent.aabb = meshComponent.mesh->data.aabb.Transform(transformComponent.globalMatrix);

                SpacePartitioning::InsertRenderableEntity(Entity(entity, &entityManager), transformComponent);
            }

            if (terrain) {
                //terrain->Update()
            }

            if (ocean) {
                //ocean->Update()
            }

        }

    }

}