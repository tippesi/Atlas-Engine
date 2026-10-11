#include "MatrixDecomposition.h"

#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace Atlas {

    namespace Common {

        // Returns (pitch, yaw, roll) such that glm::quat(angles) reproduces the rotation. Unlike glm::eulerAngles
        // this is stable close to yaw = +-90 degrees, where only pitch - roll is defined and the rotation goes into pitch
        static vec3 EulerAnglesFromRotation(const mat3& rotation) {

            // glm matrices are column-major: rotation[column][row]
            auto cosYaw = glm::sqrt(rotation[0][0] * rotation[0][0] + rotation[0][1] * rotation[0][1]);
            auto yaw = glm::atan(-rotation[0][2], cosYaw);

            if (cosYaw > 3e-4f) {
                auto pitch = glm::atan(rotation[1][2], rotation[2][2]);
                auto roll = glm::atan(rotation[0][1], rotation[0][0]);
                return vec3(pitch, yaw, roll);
            }

            auto pitch = glm::atan(-rotation[2][1], rotation[1][1]);
            return vec3(pitch, yaw, 0.0f);

        }

        MatrixDecomposition::MatrixDecomposition(const mat4& matrix) {

            Decompose(matrix);

        }

        void MatrixDecomposition::Decompose(const mat4& matrix) {

            auto local = matrix;
            auto det = glm::determinant(matrix);

            // Handle negative scales properly
            auto minIdx = 0;
            if (det < 0.0f) {
                auto sumVec = glm::vec3(1.0f) * glm::mat3(local);

                minIdx = sumVec[minIdx] < sumVec[1] ? minIdx : 1;
                minIdx = sumVec[minIdx] < sumVec[2] ? minIdx : 2;

                local[minIdx] *= -1.0f;
            }

            glm::decompose(local, scale, quaternion, translation, skew, perspective);

            if (det < 0.0f)
                scale[minIdx] *= -1.0f;

            quaternion = glm::normalize(glm::conjugate(quaternion));
            rotation = EulerAnglesFromRotation(glm::mat3_cast(quaternion));

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