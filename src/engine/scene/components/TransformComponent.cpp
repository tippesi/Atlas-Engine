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

				this->matrix = matrix;
				if (!changed)
					entity.MarkDirty();
				changed = true;
                updated = false;

			}
			
			void TransformComponent::Translate(glm::vec3 translation) {

				this->matrix = glm::translate(matrix, translation);
				if (!changed)
					entity.MarkDirty();
				changed = true;
                updated = false;

                if (!initialized) {
                    globalMatrix = matrix;
                    initialized = true;
                }

			}

			void TransformComponent::SetGlobal(const glm::mat4& matrix, const Entity& parentEntity) {

                // Motion vectors need the matrix from before the first change in this frame
                if (!lastGlobalMatrixSet) {
                    lastGlobalMatrix = globalMatrix;
                    lastGlobalMatrixSet = true;
                }

                globalMatrix = matrix;
                ReconstructLocalMatrix(parentEntity);

			}

			void TransformComponent::ReconstructLocalMatrix(const Entity& parentEntity) {

                // Uses the parent transform if possible, it already contains changes made through SetGlobal
                mat4 parentGlobalMatrix = mat4(1.0f);
                if (parentEntity.IsValid())
                    parentGlobalMatrix = parentEntity.GetScene()->GetGlobalMatrix(parentEntity);

                auto inverseParentMatrix = glm::inverse(parentGlobalMatrix);
                Set(inverseParentMatrix * globalMatrix);

			}

			void TransformComponent::Update(const glm::mat4& parentMatrix, bool parentChanged) {

                wasStatic = isStatic;

				if (parentChanged && !changed)
					entity.MarkDirty();
				changed |= parentChanged;

				if (changed) {

					globalMatrix = parentMatrix * matrix;
					inverseGlobalMatrix = mat4x3(glm::inverse(globalMatrix));

                    updated = true;

                    if (!initialized) {
                        lastGlobalMatrix = globalMatrix;
                        initialized = true;
                    }

				}

			}

            bool TransformComponent::IsStatic() const {

                return isStatic;

            }

            Common::MatrixDecomposition TransformComponent::Decompose() const {

                return Common::MatrixDecomposition(matrix);

            }

            Common::MatrixDecomposition TransformComponent::DecomposeGlobal() const {

                return Common::MatrixDecomposition(globalMatrix);

            }

		}

	}

}
