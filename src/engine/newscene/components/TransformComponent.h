#pragma once

#include "../Entity.h"
#include "../../System.h"

#include "../../common/MatrixDecomposition.h"

namespace Atlas {

	namespace NewScene {

		namespace Components {

            class TransformComponent {

            public:
                TransformComponent() = default;
                TransformComponent(const TransformComponent& that) = default;
                explicit TransformComponent(mat4 matrix) : matrix(matrix) {}

                void Translate(glm::vec3 translation);
                void Rotate(glm::vec3 rotation);
                void Scale(glm::vec3 scale);

                Common::MatrixDecomposition Decompose() const;
                void Compose(Common::MatrixDecomposition composition);

                glm::mat4 matrix;

            };

		}

	}

}