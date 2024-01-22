#include "MatrixDecomposition.h"

#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace Atlas {

    namespace Common {

        MatrixDecomposition::MatrixDecomposition(const mat4& matrix) {

            Decompose(matrix);

        }

        void MatrixDecomposition::Decompose(const mat4& matrix) {

            glm::vec3 skew;
            glm::vec4 perspective;

            glm::decompose(matrix, scale, quaternion, translation, skew, perspective);

            quaternion = glm::conjugate(quaternion);
            rotation = glm::eulerAngles(quaternion);

            quaternion = glm::normalize(quaternion);

        }

        mat4 MatrixDecomposition::Compose() {

            auto matrix = glm::translate(mat4(1.0f), translation);
            matrix = glm::scale(matrix, scale);
            matrix = matrix * glm::eulerAngleXYZ(rotation.x, rotation.y, rotation.z);

            return matrix;

        }

    }

}