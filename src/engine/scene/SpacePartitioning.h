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

			// Queries collect every entity of the cells they touch: in large worlds with many small meshes, coarse
			// cells hand thousands of candidates to each shadow pass and ray
			static constexpr float suggestedCellSize = 16.0f;

			// The smallest depth with cells of at most the suggested size
			static int32_t GetSuggestedDepth(vec3 min, vec3 max);

			static vec3 GetCellSize(vec3 min, vec3 max, int32_t depth);

            const Volume::AABB aabb;
            const float depth;

		protected:
			std::vector<Entity> QueryAABB(const Volume::AABB& aabb);

			void InsertRenderableEntity(Entity entity, const MeshComponent& transform);

			void RemoveRenderableEntity(Entity entity, const MeshComponent& transform);

			void GetRenderList(const Volume::Frustum& frustum, const Ref<RenderList::Pass>& pass);

			Scene* scene;

			Volume::Octree<Entity> renderableMovableEntityOctree;
			Volume::Octree<Entity> renderableStaticEntityOctree;

		};

	}

}