#pragma once

#include "../System.h"
#include "../ecs/EntityManager.h"

#include "components/MeshComponent.h"

namespace Atlas {

    namespace Scene {

        class Scene;

        class Entity {

        public:
            Entity() = default;
            Entity(const Entity& that) = default;
            Entity(ECS::Entity entity, ECS::EntityManager* manager) : entity(entity), entityManager(manager) {}

            template<typename Comp, typename... Args>
            inline Comp& AddComponent(Args&&... args) {

                auto& comp = entityManager->Emplace<Comp>(entity, std::forward<Args>(args)...);

                // Need to inform scene, which itself needs to take care of not yet loaded resources (like mesh, audio, etc.)
                if constexpr (std::is_same_v<Comp, Components::MeshComponent>) {
                    RegisterMeshInstance(comp);
                }

                return comp;

            }

            template<typename Comp>
            inline Comp& AddComponent(const Comp component) {

                auto& comp = entityManager->Emplace<Comp>(entity, component);

                // Need to inform scene, which itself needs to take care of not yet loaded resources (like mesh, audio, etc.)
                if constexpr (std::is_same_v<Comp, Components::MeshComponent>) {
                    RegisterMeshInstance(comp);
                }

                return comp;

            }

            template<typename Comp>
            inline void RemoveComponent() {

                assert(HasComponent<Comp>() && "Entity doesn't have this component");

                // Need to decrement resource counters
                if constexpr (std::is_same_v<Comp, Components::MeshComponent>) {
                    UnregisterMeshInstance();
                }

                entityManager->Erase<Comp>(entity);

            }

            template<typename Comp>
            inline bool HasComponent() const {

                return entityManager->Contains<Comp>(entity);

            }

            template<typename Comp>
            inline Comp& GetComponent() const {

                assert(HasComponent<Comp>() && "Entity doesn't have this component");

                return entityManager->Get<Comp>(entity);

            }

            template<typename Comp>
            inline Comp* GetComponentIfContains() const {

                assert(HasComponent<Comp>() && "Entity doesn't have this component");

                return entityManager->GetIfContains<Comp>(entity);

            }

            inline bool IsValid() const {

                return entity != ECS::EntityConfig::InvalidEntity;

            }

            operator ECS::Entity() const { return entity; }

        private:
            ECS::Entity entity = ECS::EntityConfig::InvalidEntity;
            ECS::EntityManager* entityManager = nullptr;

            void RegisterMeshInstance(const Components::MeshComponent& comp);
            void UnregisterMeshInstance();

            Scene* GetScene() const;

        };

    }

}