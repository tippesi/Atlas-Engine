#ifndef AE_ENTITY_H
#define AE_ENTITY_H

#include "../System.h"
#include "Scene.h"

namespace Atlas {

    namespace NewScene {

        class Entity {

        public:
            Entity() = default;

            Entity(ECS::Entity entity, Scene* scene) : entity(entity), scene(scene) {}

            Entity(const Entity& that) = default;

            template<typename Comp, typename... Args>
            Comp& AddComponent(Args&&... args) {

                scene->entityManager.Emplace<Comp>(entity, std::forward<Args>(args)...);

            }

            template<typename Comp>
            void RemoveComponent() {

                assert(scene->entityManager.Contains<Comp>(entity));

                scene->entityManager.Erase<Comp>(entity);

            }

            template<typename Comp>
            bool HasComponent() {

                scene->entityManager.Contains<Comp>(entity);

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

#endif
