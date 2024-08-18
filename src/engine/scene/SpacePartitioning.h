#pragma once

#include "../System.h"
#include "../volume/Octree.h"
#include "../renderer/helper/RenderList.h"

#include "components/TransformComponent.h"

namespace Atlas {

	namespace Scene {
		
		class Entity;
		class Scene;

		class SpacePartitioning {

		public:
			SpacePartitioning(Scene* scene, vec3 min, vec3 max, int32_t depth);

			void InsertRenderableEntity(Entity entity, const MeshComponent& transform);

			void RemoveRenderableEntity(Entity entity, const MeshComponent& transform);

			void GetRenderList(Volume::Frustum frustum, RenderList& renderList);

            const Volume::AABB aabb;
            const float depth;

		private:
			Scene* scene;

			Volume::Octree<ECS::Entity> renderableMovableEntityOctree;
			Volume::Octree<ECS::Entity> renderableStaticEntityOctree;

		};

	}

}