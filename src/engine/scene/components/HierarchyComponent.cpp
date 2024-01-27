#include "HierarchyComponent.h"
#include "CameraComponent.h"

namespace Atlas {

	namespace Scene {

		namespace Components {

			void HierarchyComponent::Update(const TransformComponent& transform, bool parentChanged) {

				for (auto entity : entities) {
					auto transformComponent = entity.TryGetComponent<TransformComponent>();
					auto cameraComponent = entity.TryGetComponent<CameraComponent>();
					auto hierarchyComponent = entity.TryGetComponent<HierarchyComponent>();

					if (transformComponent) {
						transformComponent->Update(transform, parentChanged);
					}

					if (cameraComponent) {
						cameraComponent->parentTransform = transformComponent ? transformComponent->globalMatrix : transform.globalMatrix;
					}

					if (hierarchyComponent) {
						hierarchyComponent->Update(transformComponent ? *transformComponent : transform, parentChanged);
					}
				}

			}

		}

	}

}