#pragma once

#include "Storage.h"
#include "Event.h"

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

            size_t Subscribe(const Topic topic, std::function<void(const Entity, Comp&)> function);

        private:
            void NotifySubscribers(const Entity entity, Comp& comp, std::vector<Subscriber<Comp>>& subscribers);

            std::vector<Comp> components;
            std::vector<Subscriber<Comp>> emplaceSubscribers;
            std::vector<Subscriber<Comp>> eraseSubscribers;

        };

        template<typename Comp>
        template<typename ...Args>
        Comp& Pool<Comp>::Emplace(const Entity entity, Args&&... args) {

            auto& comp = components.emplace_back(std::forward<Args>(args)...);
            Storage::Emplace(entity);

            NotifySubscribers(entity, comp, emplaceSubscribers);

            return comp;

        }

        template<typename Comp>
        void Pool<Comp>::Erase(const Entity entity) {

            auto idx = Storage::GetIndex(entity);

            NotifySubscribers(entity, components[idx], eraseSubscribers);

            components[idx] = std::move(components[components.size() - 1]);
            components.pop_back();

            Storage::Erase(entity);

        }

        template<typename Comp>
        Comp& Pool<Comp>::Get(const Entity entity) {

            return components[Storage::GetIndex(entity)];

        }

        template<typename Comp>
        Comp* Pool<Comp>::TryGet(const Entity entity) {

            auto idx = Storage::TryGetIndex(entity);
            if (idx == EntityConfig::InvalidEntity)
                return nullptr;

            return &components[idx];

        }

        template<typename Comp>
        size_t Pool<Comp>::Subscribe(const Topic topic, std::function<void(const Entity, Comp&)> function) {

            Subscriber<Comp> subscriber {
                .function = function
            };

            if (topic == Topic::ComponentEmplace) {
                subscriber.ID = emplaceSubscribers.size();
                emplaceSubscribers.push_back(subscriber);
            }
            else if (topic == Topic::ComponentErase) {
                subscriber.ID = eraseSubscribers.size();
                eraseSubscribers.push_back(subscriber);
            }
            else {
                AE_ASSERT(0 && "Invalid topic to subscribe to ECS pool");
            }

            return subscriber.ID;

        }

        template<typename Comp>
        void Pool<Comp>::NotifySubscribers(const Atlas::ECS::Entity entity, Comp &comp,
            std::vector<Subscriber<Comp>> &subscribers) {

            for (const auto& subscriber : subscribers) {

                subscriber.function(entity, comp);

            }

        }

    }

}