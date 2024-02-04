#include "HierarchyComponent.h"
#include "CameraComponent.h"

namespace Atlas {

	namespace Scene {

		namespace Components {

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