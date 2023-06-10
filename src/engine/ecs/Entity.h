#ifndef AE_ECSENTITY_H
#define AE_ECSENTITY_H

#include <cstdint>

namespace Atlas {

    namespace ECS {

        using Entity = uint64_t;

        struct EntityConfig {
            static constexpr Entity positionMask = Entity(0xFFFFFFFF);
            static constexpr Entity versionMask = ~Entity(0xFFFFFFFF);
            static constexpr Entity versionShift = 32;
            static constexpr Entity InvalidEntity = positionMask;
        };

        inline Entity EntityToVersion(Entity entity) {
            return (entity & EntityConfig::versionMask) >> EntityConfig::versionShift;
        }

        inline Entity EntityToPosition(Entity entity) {
            return entity & EntityConfig::positionMask;
        }

        inline uint32_t EntityToIdx(Entity entity) {
            return uint32_t(entity & EntityConfig::positionMask);
        }

        inline Entity ToEntity(Entity position, Entity version) {
            return (position & EntityConfig::positionMask) | ((version << EntityConfig::versionShift) & EntityConfig::versionMask);
        }

    }

}

#endif