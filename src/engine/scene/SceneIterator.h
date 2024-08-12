#pragma once

#include "../ecs/EntityManager.h"
#include "Entity.h"

namespace Atlas {

    namespace Scene {

        class SceneIterator : public ECS::EntityManager::Iterator {
        public:
            using ECS::EntityManager::Iterator::Iterator;

            SceneIterator(ECS::EntityManager* entityManager, const ECS::EntityManager::Iterator& iterator) 
                : ECS::EntityManager::Iterator(iterator), entityManager(entityManager) {}

            inline const Entity operator*() const {

                return { ECS::EntityManager::Iterator::operator*(), entityManager };

            }

        protected:
            ECS::EntityManager* entityManager = nullptr;

        };

    }

}