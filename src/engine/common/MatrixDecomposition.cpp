#include "MatrixDecomposition.h"

#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace Atlas {

    namespace Common {

        MatrixDecomposition::MatrixDecomposition(const mat4& matrix) {

            Decompose(matrix);

        }

        void MatrixDecomposition::Decompose(const mat4& matrix) {

            glm::decompose(matrix, scale, quaternion, translation, skew, perspective);

            quaternion = glm::conjugate(quaternion);
            rotation = glm::eulerAngles(quaternion);

            quaternion = glm::normalize(quaternion);

        }

        mat4 MatrixDecomposition::Compose() {

            mat4 matrix { 1.0f };

            matrix[0][3] = perspective.x;
            matrix[1][3] = perspective.y;
            matrix[2][3] = perspective.z;
            matrix[3][3] = perspective.w;

            matrix *= glm::translate(translation);
            matrix *= glm::mat4_cast(glm::quat(rotation));

            if (skew.x) {
                glm::mat4 skewX { 1.f };
                skewX[2][1] = skew.x;
                matrix *= skewX;
            }

            if (skew.y) {
                glm::mat4 skewY { 1.f };
                skewY[2][0] = skew.y;
                matrix *= skewY;
            }

            if (skew.z) {
                glm::mat4 skewZ { 1.f };
                skewZ[1][0] = skew.z;
                matrix *= skewZ;
            }

            matrix *= glm::scale(scale);

            return matrix;

        }

    }

}