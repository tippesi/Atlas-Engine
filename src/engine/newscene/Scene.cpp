#include "Scene.h"
#include "Entity.h"
#include "components/Components.h"

namespace Atlas {

    namespace NewScene {

        using namespace Components;

        Entity Scene::CreateEntity() {

            return Entity(entityManager.Create(), this);

        }

        void Scene::DestroyEntity(Entity entity) {

            entityManager.GetIfContains<HierarchyComponent>(entity);

            entityManager.Destroy(entity);

        }

        void Scene::Update(float deltaTime) {

            auto subset = entityManager.GetSubset<HierarchyComponent, TransformComponent>();
            for (auto entity : subset) {
                //if ()
            }

            //auto subset = entityManager.GetSubset

            if (terrain) {
                //terrain->Update()
            }

            if (ocean) {
                //ocean->Update()
            }

        }

    }

}