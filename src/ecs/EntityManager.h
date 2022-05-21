#ifndef AE_ECSENTITYMANAGER_H
#define AE_ECSENTITYMANAGER_H

#include "Entity.h"
#include "Pools.h"
#include "Subset.h"

namespace Atlas {

	namespace ECS {

		/**
		 * Class to manage entities.
		 * @note: This class is the main interaction point with the ECS.
		 */
		class EntityManager {

		public:
			/**
			 * Constructs an EntityManager object.
			 */
			EntityManager() = default;

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
			size_t Size();

			/**
			 * Returns the number of entities which are alive.
			 * @return The number of entities which are alive.
			 */
			size_t Alive();

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
			 * Returns a subset of entities which have all Comp types.
			 * @tparam Comp The component types on which entities are selected
			 * @return A subset of entities.
			 * @note A subset isn't meant to be stored. Request a new one instead.
			 */
			template<typename... Comp>
			Subset<Comp...> GetSubset();

            std::vector<Entity>::const_iterator begin() const {

                return entities.begin();

            }

            std::vector<Entity>::const_iterator end() const {

                return entities.end();

            }

		private:
			Pools pools;

			std::vector<Entity> entities;
			std::vector<Entity> destroyed;

		};

		template<typename Comp, typename ...Args>
		Comp& EntityManager::Emplace(Entity entity, Args&&... args) {

			const auto& pool = pools.Get<Comp>();

			return pool.Emplace(entity, std::forward<Args>(args)...);

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

	}

}

#endif