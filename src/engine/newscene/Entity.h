#pragma once

#include "../System.h"
#include "Scene.h"

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

                return scene->entityManager.Emplace<Comp>(entity, std::forward<Args>(args)...);

            }

            template<typename Comp>
            void RemoveComponent() {

                assert(scene->entityManager.Contains<Comp>(entity));

                scene->entityManager.Erase<Comp>(entity);

            }

            template<typename Comp>
            bool HasComponent() {

                return scene->entityManager.Contains<Comp>(entity);

            }

            template<typename Comp>
            Comp& GetComponent() const {

                return scene->entityManager.Get<Comp>(entity);

            }

            operator ECS::Entity() const { return entity; }

        private:
            ECS::Entity entity;
            Scene* scene;

        };

    }

}