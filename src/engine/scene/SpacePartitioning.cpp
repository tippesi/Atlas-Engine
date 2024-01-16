#include "SpacePartitioning.h"

#include "Entity.h"

namespace Atlas {

	namespace Scene {

		SpacePartitioning::SpacePartitioning(vec3 min, vec3 max, int32_t depth) : aabb(min, max) {

			renderableStaticEntityOctree = Volume::Octree<Entity>(aabb, depth);
			renderableMovableEntityOctree = Volume::Octree<Entity>(aabb, depth);

		}

		void SpacePartitioning::InsertRenderableEntity(Entity entity, const Components::TransformComponent& transform) {

			if (transform.isStatic) {
				renderableStaticEntityOctree.Insert(entity, transform.aabb);
			}
			else {
				renderableMovableEntityOctree.Insert(entity, transform.aabb);
			}

		}

		void SpacePartitioning::RemoveRenderableEntity(Entity entity, const Components::TransformComponent& transform) {

			if (transform.isStatic) {
				renderableStaticEntityOctree.Remove(entity, transform.aabb);
			}
			else {
				renderableMovableEntityOctree.Remove(entity, transform.aabb);
			}

		}

        void SpacePartitioning::GetRenderList(Volume::Frustum frustum, RenderList& renderList) {

            std::vector<Entity> staticEntities;
            std::vector<Entity> insideStaticEntities;
            std::vector<Entity> movableEntities;
            std::vector<Entity> insideMovableEntities;

            renderableStaticEntityOctree.QueryFrustum(staticEntities,
                insideStaticEntities, frustum);
            renderableMovableEntityOctree.QueryFrustum(movableEntities,
                insideMovableEntities, frustum);

            /*
            for (auto entity : staticEntities) {
                auto meshComp = entity.GetComponentIfContains<Components::MeshComponent>();
                if (!meshComp) continue;

                if (meshComp->dontCull || meshComp->visible && frustum.Intersects(meshComp->aabb))
                    renderList.Add(actor);
            }

            for (auto entity : insideStaticEntities) {
                auto meshComp = entity.GetComponentIfContains<Components::MeshComponent>();
                if (!meshComp) continue;

                if (actor->visible)
                    renderList.Add(actor);
            }

            for (auto entity : movableEntities) {
                auto meshComp = entity.GetComponentIfContains<Components::MeshComponent>();
                if (!meshComp) continue;

                if (actor->dontCull || actor->visible && frustum.Intersects(actor->aabb)) {
                    renderList.Add(actor);
                }
            }

            for (auto actor : insideMovableEntities) {
                auto meshComp = entity.GetComponentIfContains<Components::MeshComponent>();
                if (!meshComp) continue;

                if (entity->visible)
                    renderList.Add(actor);
            }
            */
            

        }

	}

}