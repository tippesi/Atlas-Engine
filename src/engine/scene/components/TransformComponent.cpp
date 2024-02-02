#include "TransformComponent.h"

namespace Atlas {

	namespace Scene {

		namespace Components {

			void TransformComponent::Set(const glm::mat4& matrix) {

                AE_ASSERT(!isStatic && "Change of static transform component not allowed");

                if (isStatic) {
                    return;
                }

				this->matrix = matrix;
				changed = true;
                updated = false;

			}

			void TransformComponent::Update(const TransformComponent& parentTransform, bool parentChanged) {

				lastGlobalMatrix = globalMatrix;

				if (changed || parentChanged) {

					globalMatrix = parentTransform.globalMatrix * matrix;
					inverseGlobalMatrix = mat4x3(glm::inverse(globalMatrix));

                    updated = true;

				}

			}

            bool TransformComponent::IsStatic() const {

                return isStatic;

            }

            Common::MatrixDecomposition TransformComponent::Decompose() const {

                return Common::MatrixDecomposition(matrix);

            }

		}

	}

}