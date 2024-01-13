#include "HierarchyComponent.h"

namespace Atlas {

	namespace NewScene {

		namespace Components {

			void HierarchyComponent::Update(const TransformComponent& transform, bool parentChanged) {

				for (auto entity : entities) {
					auto transformComponent = entity.GetComponentIfContains<TransformComponent>();
					auto hierarchyComponent = entity.GetComponentIfContains<HierarchyComponent>();

					if (transformComponent) {
						transformComponent->Update(transform, parentChanged);
					}

					if (hierarchyComponent) {
						hierarchyComponent->Update(transformComponent ? *transformComponent : transform, parentChanged);
					}
				}

			}

		}

	}

}