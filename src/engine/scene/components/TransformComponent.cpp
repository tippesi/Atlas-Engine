#include "TransformComponent.h"

#include "../Scene.h"

namespace Atlas {

	namespace Scene {

		namespace Components {

            TransformComponent::TransformComponent(Scene* scene, Entity entity, const TransformComponent& that) {

                if (this != &that) {
                    *this = that;
                    
                }

                this->entity = entity;

            }

			void TransformComponent::Set(const glm::mat4& matrix) {

                AE_ASSERT(!isStatic && "Change of static transform component not allowed");

                if (isStatic) {
                    return;
                }

				this->matrix = matrix;
				changed = true;
                updated = false;

			}

			void TransformComponent::ReconstructLocalMatrix(const Ref<Scene>& scene) {

                mat4 parentGlobalMatrix = mat4(1.0f);
                auto parentEntity = scene->GetParentEntity(entity);

                TransformComponent* parentTransform = nullptr;
                if (parentEntity.IsValid())
                    parentTransform = parentEntity.TryGetComponent<TransformComponent>();

                if (parentTransform) {
                    parentGlobalMatrix = parentTransform->globalMatrix;
                }

                auto inverseParentMatrix = glm::inverse(parentGlobalMatrix);
                Set(inverseParentMatrix * globalMatrix);

			}

			void TransformComponent::Update(const TransformComponent& parentTransform, bool parentChanged) {

				lastGlobalMatrix = globalMatrix;
                wasStatic = isStatic;

				changed |= parentChanged;

				if (changed) {

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