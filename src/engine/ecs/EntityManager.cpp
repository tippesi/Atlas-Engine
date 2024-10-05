#include "EntityManager.h"
#include <cassert>

namespace Atlas {

    namespace ECS {

        Entity EntityManager::Create() {

            Entity entity;

            if (destroyed.size()) {

                entity = destroyed.back();
                destroyed.pop_back();

                auto version = EntityToVersion(entity) + 1;
                auto pos = EntityToPosition(entity);

                entity = ToEntity(pos, version);

                entities[size_t(pos)] = entity;

            }
            else {

                entity = entities.emplace_back(Entity{ entities.size() });

            }

            return entity;

        }

        void EntityManager::Destroy(Entity entity) {

            auto pos = EntityToPosition(entity);

            entities[pos] = EntityConfig::InvalidEntity;

            for (auto& poolData : pools.data) {
                if (!poolData.storage)
                    continue;
                if (poolData.storage->Contains(entity))
                    poolData.storage->Erase(entity);
            }

            destroyed.emplace_back(entity);

        }

        void EntityManager::Clear() {

            entities.clear();
            destroyed.clear();

            pools.Clear();

        }

        bool EntityManager::Valid(Entity entity) {

            const auto pos = EntityToPosition(entity);
            return pos < entities.size() && entities[pos] == entity;

        }

        uint32_t EntityManager::Version(Entity entity) {

            return uint32_t(EntityToVersion(entity));

        }

        size_t EntityManager::Size() const {

            return entities.size();

        }

        size_t EntityManager::Alive() const {

            return entities.size() - destroyed.size();

        }

    }

}
