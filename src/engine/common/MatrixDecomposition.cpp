#include "MatrixDecomposition.h"

#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace Atlas {

    namespace Common {

        MatrixDecomposition::MatrixDecomposition(mat4 matrix) {

            Decompose(matrix);

        }

        void MatrixDecomposition::Decompose(mat4 matrix) {

            glm::quat rot;
            glm::vec3 skew;
            glm::vec4 perspective;

            glm::decompose(matrix, scale, rot, translation, skew, perspective);

            rot = glm::conjugate(rot);
            rotation = glm::eulerAngles(rot);

        }

        mat4 MatrixDecomposition::Compose() {

            auto matrix = glm::translate(mat4(1.0f), translation);
            matrix = glm::scale(matrix, scale);
            matrix = matrix * glm::eulerAngleXYZ(rotation.x, rotation.y, rotation.z);

            return matrix;

        }

    }

}