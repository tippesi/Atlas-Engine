#include "TransformComponent.h"

namespace Atlas {

	namespace Scene {

		namespace Components {

			void TransformComponent::Set(const glm::mat4& matrix) {

				this->matrix = matrix;
				changed = true;

			}

			void TransformComponent::Update(const TransformComponent& parentTransform, bool parentChanged) {

				lastGlobalMatrix = globalMatrix;

				if (changed || parentChanged) {

					globalMatrix = parentTransform.globalMatrix * matrix;
					inverseGlobalMatrix = mat4x3(glm::inverse(globalMatrix));

				}

			}

		}

	}

}