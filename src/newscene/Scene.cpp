#include "Scene.h"
#include "Entity.h"

namespace Atlas {

    namespace NewScene {

        Entity Scene::CreateEntity() {

            return Entity(entityManager.Create(), this);

        }

        void Scene::DestroyEntity(Entity entity) {

            entityManager.Destroy(entity);

        }

    }

}