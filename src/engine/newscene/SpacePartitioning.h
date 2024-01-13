#pragma once

#include "../System.h"
#include "../volume/Octree.h"

#include "components/TransformComponent.h"

namespace Atlas {

	namespace NewScene {
		
		class Entity;

		class SpacePartitioning {

		public:
			SpacePartitioning(vec3 min, vec3 max, int32_t depth);

			void InsertRenderableEntity(Entity entity, const Components::TransformComponent& transform);

			void RemoveRenderableEntity(Entity entity, const Components::TransformComponent& transform);

		private:
			Volume::AABB aabb;

			Volume::Octree<Entity> renderableMovableEntityOctree;
			Volume::Octree<Entity> renderableStaticEntityOctree;

		};

	}

}