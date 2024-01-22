#pragma once

#include "../ecs/EntityManager.h"
#include "Entity.h"

namespace Atlas {

    namespace Scene {

        class SceneIterator {
        public:
            SceneIterator() = delete;

            SceneIterator(ECS::EntityManager* entityManager, size_t idx)
                : entityManager(entityManager), idx(idx) {}

            SceneIterator& operator++() {
                idx++;
                return *this;
            }

            SceneIterator& operator--() {
                idx--;
                return *this;
            }

            inline bool operator!=(const SceneIterator& iterator) const {
                return idx != iterator.idx;
            }

            inline Entity operator*() const {
                return Entity(*(entityManager->begin() + idx), entityManager);
            }

        protected:
            ECS::EntityManager* entityManager = nullptr;

            size_t idx = 0;

        };

    }

}