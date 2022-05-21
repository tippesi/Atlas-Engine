#ifndef AE_SCENE_H
#define AE_SCENE_H

#include "../System.h"
#include "../ecs/EntityManager.h"

namespace Atlas {

    namespace NewScene {

        class Entity;

        class Scene {

        public:
            Scene() = default;
            Scene(const Scene& that) = default;
            explicit Scene(const std::string& name) : name(name) {}

            Entity CreateEntity();

            void DestroyEntity(Entity entity);

            void Update(float deltaTime);

            std::string name;

        private:
            ECS::EntityManager entityManager;

            friend class Entity;
            friend class SceneSerializer;

        };

    }

}

#endif