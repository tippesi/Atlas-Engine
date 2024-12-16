#include "HierarchyComponent.h"
#include "CameraComponent.h"

#include "../Scene.h"

namespace Atlas {

	namespace Scene {

		namespace Components {

			void HierarchyComponent::AddChild(Entity entity) {

				AE_ASSERT(scene != nullptr && "Hierarchy component needs to be added to entity before inserting children");
				AE_ASSERT(!scene->childToParentMap.contains(entity) && "Entity can't be part of more than one hierarchy");

				scene->childToParentMap[entity] = owningEntity;
				entities.push_back(entity);

			}

			void HierarchyComponent::RemoveChild(Entity entity) {

				auto it = std::find(entities.begin(), entities.end(), entity);

				if (it != entities.end()) {
					scene->childToParentMap.erase(entity);
					entities.erase(it);
				}

			}

			std::vector<Entity>& HierarchyComponent::GetChildren() {

				return entities;

			}

			void HierarchyComponent::Update(const TransformComponent& transform, bool parentChanged,
				ECS::Pool<TransformComponent>& transformComponentPool,
				ECS::Pool<HierarchyComponent>& hierarchyComponentPool, ECS::Pool<CameraComponent>& cameraComponentPool) {

				updated = true;

				globalMatrix = transform.globalMatrix;

				for (auto entity : entities) {
					bool transformChanged = parentChanged;

					auto transformComponent = transformComponentPool.TryGet(entity);
					auto cameraComponent = cameraComponentPool.TryGet(entity);
					auto hierarchyComponent = hierarchyComponentPool.TryGet(entity);

					if (transformComponent) {
						transformChanged |= transformComponent->changed;
						transformComponent->Update(transform, parentChanged);
					}

					if (hierarchyComponent) {
						AE_ASSERT(!hierarchyComponent->root && "A child hierarchy should never also be a root hierarchy");
						hierarchyComponent->Update(transformComponent ? *transformComponent : transform,
							transformChanged, transformComponentPool, hierarchyComponentPool, cameraComponentPool);
					}
				}

			}

			void HierarchyComponent::Update(JobGroup& jobGroup, const TransformComponent& transform, bool parentChanged,
				ECS::Pool<TransformComponent>& transformComponentPool,
				ECS::Pool<HierarchyComponent>& hierarchyComponentPool, ECS::Pool<CameraComponent>& cameraComponentPool) {

                updated = true;

				globalMatrix = transform.globalMatrix;

				if (entities.size() < 128) {

					for (auto entity : entities) {
						bool transformChanged = parentChanged;

						auto transformComponent = transformComponentPool.TryGet(entity);
						auto cameraComponent = cameraComponentPool.TryGet(entity);
						auto hierarchyComponent = hierarchyComponentPool.TryGet(entity);

						if (transformComponent) {
							transformChanged |= transformComponent->changed;
							transformComponent->Update(transform, parentChanged);
						}

						if (hierarchyComponent) {
							AE_ASSERT(!hierarchyComponent->root && "A child hierarchy should never also be a root hierarchy");
							hierarchyComponent->Update(jobGroup, transformComponent ? *transformComponent : transform,
								transformChanged, transformComponentPool, hierarchyComponentPool, cameraComponentPool);
						}
					}
				}
				else {
					JobSystem::ParallelFor(jobGroup, int32_t(entities.size()), 8, 
						[&, parentChanged](JobData& data, int32_t idx) {
							auto entity = entities[idx];
							bool transformChanged = parentChanged;

							auto transformComponent = transformComponentPool.TryGet(entity);
							auto cameraComponent = cameraComponentPool.TryGet(entity);
							auto hierarchyComponent = hierarchyComponentPool.TryGet(entity);

							if (transformComponent) {
								transformChanged |= transformComponent->changed;
								transformComponent->Update(transform, parentChanged);
							}

							if (hierarchyComponent) {
								AE_ASSERT(!hierarchyComponent->root && "A child hierarchy should never also be a root hierarchy");
								hierarchyComponent->Update(jobGroup, transformComponent ? *transformComponent : transform,
									transformChanged, transformComponentPool, hierarchyComponentPool, cameraComponentPool);
							}
						});
				}

			}

			void HierarchyComponent::ProcessChild(Entity entity, const TransformComponent& transform, bool parentChanged, 
				JobGroup& jobGroup, ECS::Pool<TransformComponent>& transformComponentPool,
				ECS::Pool<HierarchyComponent>& hierarchyComponentPool, ECS::Pool<CameraComponent>& cameraComponentPool) {

				

			}

		}

	}

}