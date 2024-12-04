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

			void HierarchyComponent::Update(const TransformComponent& transform, bool parentChanged) {

                updated = true;

				globalMatrix = transform.globalMatrix;

				auto& transformComponentPool = scene->entityManager.GetPool<TransformComponent>();
				auto& hierarchyComponentPool = scene->entityManager.GetPool<HierarchyComponent>();
				auto& cameraComponentPool = scene->entityManager.GetPool<CameraComponent>();

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
                            transformChanged);
					}
				}

			}

		}

	}

}