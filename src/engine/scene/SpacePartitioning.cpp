#include "SpacePartitioning.h"
#include "Scene.h"

#include "Entity.h"

namespace Atlas {

	namespace Scene {

		SpacePartitioning::SpacePartitioning(Scene* scene, vec3 min, vec3 max, int32_t depth) :
            scene(scene), aabb(min, max), depth(depth) {

			renderableStaticEntityOctree = Volume::Octree<ECS::Entity>(aabb, depth);
			renderableMovableEntityOctree = Volume::Octree<ECS::Entity>(aabb, depth);

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

        void SpacePartitioning::GetRenderList(Volume::Frustum frustum, RenderList& renderList) {

            auto entityManager = &scene->entityManager;

            std::vector<ECS::Entity> staticEntities;
            std::vector<ECS::Entity> insideStaticEntities;
            std::vector<ECS::Entity> movableEntities;
            std::vector<ECS::Entity> insideMovableEntities;

            renderableStaticEntityOctree.QueryFrustum(staticEntities,
                insideStaticEntities, frustum);
            renderableMovableEntityOctree.QueryFrustum(movableEntities,
                insideMovableEntities, frustum);

            /*
            for (auto entity : staticEntities) {
                auto meshComp = entityManager->TryGet<MeshComponent>(entity);
                if (!meshComp) continue;

                if (meshComp->dontCull || meshComp->visible && frustum.Intersects(meshComp->aabb))
                    renderList.Add(entity, *meshComp);
            }

            for (auto entity : insideStaticEntities) {
                auto meshComp = entityManager->TryGet<MeshComponent>(entity);
                if (!meshComp) continue;

                if (meshComp->visible)
                    renderList.Add(entity, *meshComp);
            }

            for (auto entity : movableEntities) {
                auto meshComp = entityManager->TryGet<MeshComponent>(entity);
                if (!meshComp) continue;

                if (meshComp->dontCull || meshComp->visible && frustum.Intersects(meshComp->aabb))
                    renderList.Add(entity, *meshComp);
            }

            for (auto entity : insideMovableEntities) {
                auto meshComp = entityManager->TryGet<MeshComponent>(entity);
                if (!meshComp) continue;

                if (meshComp->visible)
                    renderList.Add(entity, *meshComp);
            }
            */

        }

	}

}