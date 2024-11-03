#pragma once

#include "Entity.h"
#include "Pools.h"
#include "Subset.h"

#include <optional>

namespace Atlas {

    namespace ECS {

        /**
         * Class to manage entities.
         * @note: This class is the main interaction point with the ECS.
         */
        class EntityManager {

        public:
            class Iterator {

            public:
                Iterator() = delete;

                Iterator(const std::vector<Entity>* entities, size_t idx) : idx(idx), entities(entities) {

                    if (idx < entities->size() && !ValidEntity()) {
                        operator++();
                    }

                }

                Iterator& operator++() {

                    while (++idx < entities->size() && !ValidEntity());
                    return *this;

                }

                Iterator& operator--() {

                    while (--idx > 0 && !ValidEntity());
                    return *this;

                }

                inline bool operator!=(const Iterator& iterator) const {

                    return idx != iterator.idx;

                }

                inline const Entity& operator*() const {

                    return (*entities)[idx];

                }

            protected:
                bool ValidEntity() const {
                    return (*entities)[idx] != EntityConfig::InvalidEntity;
                }
               
                size_t idx = 0;

                const std::vector<Entity>* entities;

            };

            /**
             * Constructs an EntityManager object.
             */
            EntityManager() = default;

            /**
             * Constructs an EntityManager object with userData.
             */
            explicit EntityManager(void* userData) : userData(userData) {}

            /**
             * Creates a new entity
             * @return The new entity.
             */
            Entity Create();

            /**
             * Destroy an entity.
             * @param entity The entity to be destroyed.
             * @note A destroyed entity might be recycled later on.
             * The current entity handle will be invalid.
             */
            void Destroy(Entity entity);

            /**
             * Destroys all data inside the entity manager
             */
            void Clear();

            /**
             * Checks whether an entity is valid.
             * @param entity The entity to be checked.
             * @return True if valid, false otherwise
             * @note Only valid entities can be used to attach new components to.
             */
            bool Valid(Entity entity);

            /**
             * Gets the version number of the current entity.
             * @param entity The entity to be checked
             * @return The version number of the entity.
             * @note The version number indicates the number of times
             * that the entity was recycled.
             */
            uint32_t Version(Entity entity);

            /**
             * Returns the size of the entity pool.
             * @return The size of the entity pool
             * @note The size of the entity pool depends only
             * on the maximum number of entities created. For
             * now it will not shrink.
             */
            size_t Size() const;

            /**
             * Returns the number of entities which are alive.
             * @return The number of entities which are alive.
             */
            size_t Alive() const;

            /**
             * Creates a component and associates it with an entity.
             * @tparam Comp The component type
             * @tparam Args The types of the constructor argument
             * @param entity The entity which is associated with the component
             * @param args The arguments to construct the component
             * @return Returns a reference to the newly created component
             */
            template<typename Comp, typename ...Args>
            Comp& Emplace(Entity entity, Args&&... args);

            /**
             * Replaces a component and associates it with an entity.
             * @tparam Comp The component type
             * @param entity The entity which is associated with the component
             * @param comp The component to add
             * @return Returns a reference to the newly created component
             */
            template<typename Comp>
            Comp& Replace(Entity entity, Comp& comp);

            /**
             * Erases a component.
             * @tparam Comp The component type.
             * @param entity The entity which associated component should be erased.
             */
            template<typename Comp>
            void Erase(Entity entity);

            /**
             * Checks whether or not an entity contains a component.
             * @tparam Comp The component type to check.
             * @param entity The entity to check.
             * @return True if the entity contains the component, false otherwise
             */
            template<typename... Comp>
            bool Contains(Entity entity);

            /**
             * Returns the component which is associated with the entity.
             * @tparam Comp The component type
             * @param entity The entity
             * @return A reference to the component.
             */
            template<typename Comp>
            Comp& Get(Entity entity);

            /**
             * Returns the component which is associated with the entity if the entity has the component.
             * @tparam Comp The component type
             * @param entity The entity
             * @return A pointer to the component if valid, nullptr if it wasn't found.
             * @note The pointer is only valid temporarily
             */
            template<typename Comp>
            Comp* TryGet(Entity entity);

            /**
             * Returns a subset of entities which have all Comp types.
             * @tparam Comp The component types on which entities are selected
             * @return A subset of entities.
             * @note A subset isn't meant to be stored. Request a new one instead.
             */
            template<typename... Comp>
            Subset<Comp...> GetSubset();

            /**
             * Returns a reference to the internal vector of components of type Comp.
             * @note Actually changing this vector in terms of content might lead to issues. Prefer
             * to create a copy instead of a reference. 
             */
            template<typename Comp>
            std::vector<Comp>& GetAll();

            /*
             * Returns the amount of components for a type Comp
             */
            template<typename Comp>
            size_t GetCount();

            template<typename Comp>
            size_t SubscribeToTopic(const Topic topic, std::function<void(const Entity, Comp&)> function);

            Iterator begin() const {

                return { &entities, 0 };

            }

            Iterator end() const {

                return { &entities, entities.size() };

            }

            void* userData;

        private:
            Pools pools;

            std::vector<Entity> entities;
            std::vector<Entity> destroyed;

        };

        template<typename Comp, typename ...Args>
        Comp& EntityManager::Emplace(Entity entity, Args&&... args) {

            auto& pool = pools.Get<Comp>();

            return pool.Emplace(entity, std::forward<Args>(args)...);

        }

        template<typename Comp>
        Comp& EntityManager::Replace(Entity entity, Comp& comp) {

            auto& pool = pools.Get<Comp>();

            return pool.Replace(entity, comp);

        }

        template<typename Comp>
        void EntityManager::Erase(Entity entity) {

            auto& pool = pools.Get<Comp>();

            pool.Erase(entity);

        }

        template<typename... Comp>
        bool EntityManager::Contains(Entity entity) {

            return (pools.Get<Comp>().Contains(entity) && ...);

        }

        template<typename Comp>
        Comp& EntityManager::Get(Entity entity) {

            auto& pool = pools.Get<Comp>();

            return pool.Get(entity);

        }

        template<typename Comp>
        Comp* EntityManager::TryGet(Entity entity) {

            auto& pool = pools.Get<Comp>();

            return pool.TryGet(entity);

        }

        template<typename... Comp>
        Subset<Comp...> EntityManager::GetSubset() {

            return { std::tuple { &pools.Get<std::decay_t<Comp>>()..., } };

            /*
            if constexpr (sizeof...(Comp) == 1) {
                return (pool.Get<Comp>(), ...);
            }
            else {
                return std::forward_as_tuple(Subset<Comp>()...);
            }
            */

        }

        template<typename Comp>
        std::vector<Comp>& EntityManager::GetAll() {

            return pools.Get<Comp>().GetAll();

        }

        template<typename Comp>
        size_t EntityManager::GetCount() {

            return pools.Get<Comp>().GetCount();

        }

        template<typename Comp>
        size_t EntityManager::SubscribeToTopic(const Topic topic,
            std::function<void(const Entity, Comp&)> function) {

            auto& pool = pools.Get<Comp>();

            return pool.Subscribe(topic, function);

        }

    }

}