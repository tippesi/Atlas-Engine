#include "SpacePartitioning.h"
#include "Scene.h"

#include "Entity.h"

namespace Atlas {

	namespace Scene {

		SpacePartitioning::SpacePartitioning(Scene* scene, vec3 min, vec3 max, int32_t depth) :
            scene(scene), aabb(min, max), depth(depth) {

			renderableStaticEntityOctree = Volume::Octree<Entity>(aabb, depth);
			renderableMovableEntityOctree = Volume::Octree<Entity>(aabb, depth);

		}

        int32_t SpacePartitioning::GetSuggestedDepth(vec3 min, vec3 max) {

            auto size = max - min;
            auto largest = glm::max(size.x, glm::max(size.y, size.z));
            return glm::max(1, int32_t(glm::ceil(glm::log2(largest / suggestedCellSize))));

        }

        vec3 SpacePartitioning::GetCellSize(vec3 min, vec3 max, int32_t depth) {

            return (max - min) / glm::pow(2.0f, float(depth));

        }

        std::vector<Entity> SpacePartitioning::QueryAABB(const Volume::AABB& aabb) {

            std::vector<Entity> entities;
            renderableStaticEntityOctree.QueryAABB(entities, aabb);
            renderableMovableEntityOctree.QueryAABB(entities, aabb);

            return entities;

        }

		void SpacePartitioning::InsertRenderableEntity(Entity entity, const MeshComponent& transform) {

			if (false) {
				renderableStaticEntityOctree.Insert(entity, transform.aabb);
			}
			else {
				renderableMovableEntityOctree.Insert(entity, transform.aabb);
			}

		}

		void SpacePartitioning::RemoveRenderableEntity(Entity entity, const MeshComponent& transform) {

			if (false) {
				renderableStaticEntityOctree.Remove(entity, transform.aabb);
			}
			else {
				renderableMovableEntityOctree.Remove(entity, transform.aabb);
			}

		}

	}

}