#pragma once

#include "Storage.h"

#include <vector>

namespace Atlas {

    namespace ECS {

        template<typename Comp>
        class Pool : public Storage {

        public:
            Pool() = default;

            template<typename... Args>
            Comp& Emplace(const Entity entity, Args&&... args);

            void Erase(const Entity entity) override;

            Comp& Get(const Entity entity);

            Comp* TryGet(const Entity entity);

        private:
            std::vector<Comp> components;

        };

        template<typename Comp>
        template<typename ...Args>
        Comp& Pool<Comp>::Emplace(const Entity entity, Args&&... args) {

            auto& comp = components.emplace_back(std::forward<Args>(args)...);
            Storage::Emplace(entity);

            return comp;

        }

        template<typename Comp>
        void Pool<Comp>::Erase(const Entity entity) {

            auto idx = Storage::GetIndex(entity);

            components[idx] = std::move(components.back());
            components.pop_back();

            Storage::Erase(entity);

        }

        template<typename Comp>
        Comp& Pool<Comp>::Get(const Entity entity) {

            return components[Storage::GetIndex(entity)];

        }

        template<typename Comp>
        Comp* Pool<Comp>::TryGet(const Entity entity) {

            auto idx = Storage::GetIndex(entity);
            if (idx == EntityConfig::InvalidEntity)
                return nullptr;

            return &components[idx];

        }

    }

}