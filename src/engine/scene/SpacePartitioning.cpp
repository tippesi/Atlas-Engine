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

        void SpacePartitioning::GetRenderList(Volume::Frustum frustum, const Ref<RenderList::Pass>& pass) {

            
            auto entityManager = &scene->entityManager;

            std::vector<Entity> staticEntities;
            std::vector<Entity> insideStaticEntities;
            std::vector<Entity> movableEntities;
            std::vector<Entity> insideMovableEntities;

            renderableStaticEntityOctree.QueryFrustum(staticEntities,
                insideStaticEntities, frustum);
            renderableMovableEntityOctree.QueryFrustum(movableEntities,
                insideMovableEntities, frustum);

            for (auto entity : staticEntities) {
                auto meshComp = entityManager->TryGet<MeshComponent>(entity);
                if (!meshComp) continue;

                if (meshComp->dontCull || meshComp->visible && frustum.Intersects(meshComp->aabb))
                    pass->Add(entity, *meshComp);
            }

            for (auto entity : insideStaticEntities) {
                auto meshComp = entityManager->TryGet<MeshComponent>(entity);
                if (!meshComp) continue;

                if (meshComp->visible)
                    pass->Add(entity, *meshComp);
            }

            for (auto entity : movableEntities) {
                auto meshComp = entityManager->TryGet<MeshComponent>(entity);
                if (!meshComp) continue;

                if (meshComp->dontCull || meshComp->visible && frustum.Intersects(meshComp->aabb))
                    pass->Add(entity, *meshComp);
            }

            for (auto entity : insideMovableEntities) {
                auto meshComp = entityManager->TryGet<MeshComponent>(entity);
                if (!meshComp) continue;

                if (meshComp->visible)
                    pass->Add(entity, *meshComp);
            }


        }

	}

}