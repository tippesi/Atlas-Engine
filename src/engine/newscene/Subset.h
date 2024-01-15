#pragma once

#include "../ecs/Subset.h"

#include "Entity.h"

namespace Atlas {

    namespace NewScene {

        template<typename... Comp>
        class Subset {

            using OtherStorage = std::array<const ECS::Storage*, (sizeof...(Comp) - 1)>;

        public:
            class Iterator : public ECS::Subset<Comp...>::Iterator {

            public:
                Iterator(const ECS::Storage* mainStorage, OtherStorage otherStorages, size_t idx,
                    ECS::EntityManager* entityManager) : entityManager(entityManager),
                    ECS::Subset<Comp...>::Iterator(mainStorage, otherStorages, idx) {}

                inline const Entity& operator*() const {

                    return { ECS::Subset<Comp...>::Iterator::entity, entityManager };

                }

            private:
                ECS::EntityManager* entityManager;

            };

            Subset(const ECS::Subset<Comp...>& subset) : subset(subset) {}

            Iterator begin() const {

                return { subset.mainStorage, subset.otherStorages, 0, entityManager };

            }

            Iterator end() const {

                return { subset.mainStorage, subset.otherStorages, subset.mainStorage->Size(), entityManager };

            }

        private:
            ECS::Subset<Comp...> subset;
            ECS::EntityManager* entityManager;

        };

    }

}