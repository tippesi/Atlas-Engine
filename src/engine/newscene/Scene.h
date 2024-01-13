#pragma once

#include "../System.h"
#include "../ecs/EntityManager.h"

#include "../ocean/Ocean.h"
#include "../terrain/Terrain.h"

#include <type_traits>

namespace Atlas {

    namespace NewScene {

        class Entity;

        class Scene {

        public:
            Scene() = default;
            Scene(const Scene& that) = default;
            explicit Scene(const std::string& name) : name(name) {}

            Entity CreateEntity();

            template<typename T, typename ...Args>
            T CreatePrefab(Args&&... args);

            void DestroyEntity(Entity entity);

            void Update(float deltaTime);

            std::string name;

            Ref<Ocean::Ocean> ocean;
            Ref<Terrain::Terrain> terrain;

        private:
            ECS::EntityManager entityManager;

            friend class Entity;
            friend class SceneSerializer;

        };

        template<typename T, typename ...Args>
        T Scene::CreatePrefab(Args&&... args) {

            static_assert(std::is_convertible<T, Entity>(),
                "Prefab needs to inherit from Scene::Entity class without any extra members");
            static_assert(std::is_constructible<T, ECS::Entity, Scene*, Args...>(),
                "Can't construct prefab with given arguments. Prefab needs to have at \
                least ECS::Entity and Scene* as constructor arguments.");

            return T(entityManager.Create(), this, std::forward<Args>(args)...);

        }

    }

}