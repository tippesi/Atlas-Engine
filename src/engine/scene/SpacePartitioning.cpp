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

        void SpacePartitioning::GetRenderList(const Volume::Frustum& frustum, const Ref<RenderList::Pass>& pass) {
            
            auto entityManager = &scene->entityManager;

            std::vector<Entity> staticEntities;
            std::vector<Entity> insideStaticEntities;
            std::vector<Entity> movableEntities;
            std::vector<Entity> insideMovableEntities;

            renderableStaticEntityOctree.QueryFrustum(staticEntities,
                insideStaticEntities, frustum);
            renderableMovableEntityOctree.QueryFrustum(movableEntities,
                insideMovableEntities, frustum);

            // Shadow passes drop meshes without shadows later on, don't collect them in the first place
            auto shadowPass = pass->type == RenderList::RenderPassType::Shadow;
            auto getMeshComponent = [&](Entity entity) -> MeshComponent* {
                auto meshComp = entityManager->TryGet<MeshComponent>(entity);
                if (!meshComp || shadowPass && meshComp->mesh.IsLoaded() && !meshComp->mesh->castShadow)
                    return nullptr;
                return meshComp;
            };

            for (auto entity : staticEntities) {
                auto meshComp = getMeshComponent(entity);
                if (!meshComp) continue;

                if (meshComp->dontCull || meshComp->visible && frustum.Intersects(meshComp->aabb))
                    pass->Add(entity, *meshComp);
            }

            for (auto entity : insideStaticEntities) {
                auto meshComp = getMeshComponent(entity);
                if (!meshComp) continue;

                if (meshComp->visible)
                    pass->Add(entity, *meshComp);
            }

            for (auto entity : movableEntities) {
                auto meshComp = getMeshComponent(entity);
                if (!meshComp) continue;

                if (meshComp->dontCull || meshComp->visible && frustum.Intersects(meshComp->aabb))
                    pass->Add(entity, *meshComp);
            }

            for (auto entity : insideMovableEntities) {
                auto meshComp = getMeshComponent(entity);
                if (!meshComp) continue;

                if (meshComp->visible)
                    pass->Add(entity, *meshComp);
            }


        }

	}

}