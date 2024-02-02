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

                AE_ASSERT(!HasComponent<Comp>() && "Entity already has this component");

                if constexpr (std::is_constructible_v<Comp, Scene*, Args...>) {
                    return entityManager->Emplace<Comp>(entity,
                        static_cast<Scene*>(entityManager->userData), std::forward<Args>(args)...);
                }
                else {
                    return entityManager->Emplace<Comp>(entity, std::forward<Args>(args)...);
                }

            }

            template<typename Comp>
            inline void RemoveComponent() {

                AE_ASSERT(HasComponent<Comp>() && "Entity doesn't have this component");

                entityManager->Erase<Comp>(entity);

            }

            template<typename Comp>
            inline Comp& ReplaceComponent(Comp& comp) {

                return entityManager->Replace<Comp>(entity, comp);

            }

            template<typename Comp>
            inline bool HasComponent() const {

                return entityManager->Contains<Comp>(entity);

            }

            template<typename Comp>
            inline Comp& GetComponent() const {

                AE_ASSERT(HasComponent<Comp>() && "Entity doesn't have this component");

                return entityManager->Get<Comp>(entity);

            }

            template<typename Comp>
            inline Comp* TryGetComponent() const {

                return entityManager->TryGet<Comp>(entity);

            }

            inline bool IsValid() const {

                return entity != ECS::EntityConfig::InvalidEntity;

            }

            operator ECS::Entity() const { return entity; }

        private:
            ECS::Entity entity = ECS::EntityConfig::InvalidEntity;
            ECS::EntityManager* entityManager = nullptr;

        };

    }

}