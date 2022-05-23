#include "Scene.h"
#include "Entity.h"
#include "Components.h"

namespace Atlas {

    namespace NewScene {

        Entity Scene::CreateEntity() {

            return Entity(entityManager.Create(), this);

        }

        void Scene::DestroyEntity(Entity entity) {

            entityManager.Destroy(entity);

        }

        void Scene::Update(float deltaTime) {

            if (terrain) {
                //terrain->Update()
            }

            if (ocean) {
                //ocean->Update()
            }

            auto subset = entityManager.GetSubset<HierarchyComponent>();
            for (auto entity : subset) {
                
            }

        }

    }

}