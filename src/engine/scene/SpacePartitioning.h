#pragma once

#include "../System.h"
#include "../volume/Octree.h"
#include "../RenderList.h"

#include "components/TransformComponent.h"

namespace Atlas {

	namespace Scene {
		
		class Entity;

		class SpacePartitioning {

		public:
			SpacePartitioning(vec3 min, vec3 max, int32_t depth);

			void InsertRenderableEntity(Entity entity, const Components::MeshComponent& transform);

			void RemoveRenderableEntity(Entity entity, const Components::MeshComponent& transform);

			void GetRenderList(Volume::Frustum frustum, RenderList& renderList);

		private:
			Volume::AABB aabb;

			Volume::Octree<Entity> renderableMovableEntityOctree;
			Volume::Octree<Entity> renderableStaticEntityOctree;

		};

	}

}