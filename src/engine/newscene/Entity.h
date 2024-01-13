#pragma once

#include "../System.h"
#include "../ecs/EntityManager.h"

#include "Scene.h"
#include "components/MeshComponent.h"

namespace Atlas {

    namespace NewScene {

        class Entity {

        public:
            Entity() = default;
            Entity(const Entity& that) = default;
            Entity(ECS::Entity entity, Scene* scene) : entity(entity), scene(scene) {}

            Entity Get() const {

                return *this;

            }

            template<typename Comp, typename... Args>
            Comp& AddComponent(Args&&... args) {

                // Need to inform scene, which itself needs to take care of not yet loaded resources (like mesh, audio, etc.)
                if constexpr (std::is_same_v<Comp, Components::MeshComponent>) {
                    scene->newMeshComponentEntities.push_back(*this);
                }

                return scene->entityManager.Emplace<Comp>(entity, std::forward<Args>(args)...);

            }

            template<typename Comp>
            void RemoveComponent() {

                assert(HasComponent<Comp>() && "Entity doesn't have this component");

                // Need to decrement resource counters
                if constexpr (std::is_same_v<Comp, Components::MeshComponent>) {
                    auto& meshComponent = GetComponent<Components::MeshComponent>();

                    if (meshComponent.mesh.IsValid() && scene->registeredMeshes.contains(meshComponent.mesh.GetID()))
                        scene->registeredMeshes[meshComponent.mesh.GetID()].refCount--;
                }

                scene->entityManager.Erase<Comp>(entity);

            }

            template<typename Comp>
            bool HasComponent() const {

                return scene->entityManager.Contains<Comp>(entity);

            }

            template<typename Comp>
            Comp& GetComponent() const {

                assert(HasComponent<Comp>() && "Entity doesn't have this component");

                return scene->entityManager.Get<Comp>(entity);

            }

            template<typename Comp>
            Comp* GetComponentIfContains() const {

                assert(HasComponent<Comp>() && "Entity doesn't have this component");

                return scene->entityManager.GetIfContains<Comp>(entity);

            }

            operator ECS::Entity() const { return entity; }

        private:
            ECS::Entity entity;
            Scene* scene;

        };

    }

}