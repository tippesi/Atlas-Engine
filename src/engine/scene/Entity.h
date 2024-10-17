#pragma once

#include "../System.h"
#include "../ecs/EntityManager.h"

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

                AE_ASSERT(IsValid() && "Entity is not valid and doesn't belong to any scene");

                AE_ASSERT(!HasComponent<Comp>() && "Entity already has this component");

                if constexpr (std::is_constructible_v<Comp, Scene*, Entity, Args...>) {
                    return entityManager->Emplace<Comp>(entity,
                        static_cast<Scene*>(entityManager->userData), *this, std::forward<Args>(args)...);
                }
                else if constexpr (std::is_constructible_v<Comp, Scene*, Args...>) {
                    return entityManager->Emplace<Comp>(entity,
                        static_cast<Scene*>(entityManager->userData), std::forward<Args>(args)...);
                }
                else {
                    return entityManager->Emplace<Comp>(entity, std::forward<Args>(args)...);
                }

            }

            template<typename Comp>
            inline void RemoveComponent() {

                AE_ASSERT(IsValid() && "Entity is not valid and doesn't belong to any scene");
                AE_ASSERT(HasComponent<Comp>() && "Entity doesn't have this component");

                entityManager->Erase<Comp>(entity);

            }

            template<typename Comp>
            inline bool HasComponent() const {

                AE_ASSERT(IsValid() && "Entity is not valid and doesn't belong to any scene");

                return entityManager->Contains<Comp>(entity);

            }

            template<typename Comp>
            inline Comp& GetComponent() const {

                AE_ASSERT(IsValid() && "Entity is not valid and doesn't belong to any scene");
                AE_ASSERT(HasComponent<Comp>() && "Entity doesn't have this component");

                return entityManager->Get<Comp>(entity);

            }

            template<typename Comp>
            inline Comp* TryGetComponent() const {

                AE_ASSERT(IsValid() && "Entity is not valid and doesn't belong to any scene");

                return entityManager->TryGet<Comp>(entity);

            }

            inline bool IsValid() const {

                return entity != ECS::EntityConfig::InvalidEntity;

            }

            inline uint64_t GetID() const {

                return uint64_t(entity);

            }

            inline uint32_t GetVersion() const {

                return entityManager->Version(entity);

            }

            inline Scene* GetScene() const {

                return static_cast<Scene*>(entityManager->userData);

            }

            operator ECS::Entity() const { return entity; }

            static std::vector<uint8_t> Backup(const Ref<Scene>& scene, const Entity& entity);

            static Entity Restore(const Ref<Scene>& scene, const std::vector<uint8_t>& serialized);

        private:
            ECS::Entity entity = ECS::EntityConfig::InvalidEntity;
            ECS::EntityManager* entityManager = nullptr;

            friend Scene;

        };

    }

}