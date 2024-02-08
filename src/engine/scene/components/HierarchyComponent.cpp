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

				for (auto entity : entities) {
                    bool transformChanged = parentChanged;

					auto transformComponent = entity.TryGetComponent<TransformComponent>();
					auto cameraComponent = entity.TryGetComponent<CameraComponent>();
					auto hierarchyComponent = entity.TryGetComponent<HierarchyComponent>();

					if (transformComponent) {
                        transformChanged |= transformComponent->changed;
						transformComponent->Update(transform, parentChanged);
					}

					if (hierarchyComponent) {
						hierarchyComponent->Update(transformComponent ? *transformComponent : transform,
                            transformChanged);
					}
				}

			}

		}

	}

}