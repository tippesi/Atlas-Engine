#pragma once

#include "../ecs/Subset.h"

#include "Entity.h"

namespace Atlas {

    namespace Scene {

        template<typename... Comp>
        class Subset {

            using OtherStorage = std::array<const ECS::Storage*, (sizeof...(Comp) - 1)>;

        public:
            class Iterator : public ECS::Subset<Comp...>::Iterator {

            public:
                Iterator(const ECS::Storage* mainStorage, OtherStorage otherStorages, size_t idx,
                    ECS::EntityManager* entityManager) : entityManager(entityManager),
                    ECS::Subset<Comp...>::Iterator(mainStorage, otherStorages, idx) {}

                inline const Entity operator*() const {

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

            template<typename... Component>
            decltype(auto) Get(const Entity entity) {

                return subset.template Get<Component...>(entity);

            }

            bool Any() const {

                return subset.Any();

            }

            ECS::Subset<Comp...> subset;
            ECS::EntityManager* entityManager;

        };

    }

}