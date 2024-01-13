#pragma once

#include "../System.h"
#include "../ecs/EntityManager.h"
#include "../resource/ResourceManager.h"

#include "../ocean/Ocean.h"
#include "../terrain/Terrain.h"

#include "../mesh/Mesh.h"

#include "SpacePartitioning.h"

#include <type_traits>
#include <map>

namespace Atlas {

    namespace NewScene {

        class Entity;

        class Scene : public SpacePartitioning {

            template<typename T>
            struct RegisteredResource {
                ResourceHandle<T> resource;
                uint32_t refCount = 0;
            };

        public:
            Scene() : SpacePartitioning(vec3(-2048.0f), vec3(2048.0f), 5) {};
            Scene(const Scene& that) = default;
            explicit Scene(const std::string& name) 
                : name(name), SpacePartitioning(vec3(-2048.0f), vec3(2048.0f), 5) {}
            explicit Scene(const std::string& name, vec3 min, vec3 max, int32_t depth = 5) 
                : name(name), SpacePartitioning(min, max, depth) {}

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

            std::map<Hash, RegisteredResource<Mesh::Mesh>> registeredMeshes;

            std::vector<Entity> newMeshComponentEntities;

            friend class Entity;
            friend class SceneSerializer;

        };

        template<typename T, typename ...Args>
        T Scene::CreatePrefab(Args&&... args) {

            static_assert(std::is_convertible<T, Entity>() && sizeof(T) == sizeof(Entity),
                "Prefab needs to inherit from Scene::Entity class without any extra members");
            static_assert(std::is_constructible<T, ECS::Entity, Scene*, Args...>(),
                "Can't construct prefab with given arguments. Prefab needs to have at \
                least ECS::Entity and Scene* as constructor arguments.");

            return T(entityManager.Create(), this, std::forward<Args>(args)...);

        }

    }

}