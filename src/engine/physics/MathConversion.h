#pragma once

#include "../System.h"
#include "../common/MatrixDecomposition.h"

#include <Jolt/Jolt.h>

namespace Atlas {

    namespace Physics {

        inline void MatrixToJPHPosAndRot(const mat4& matrix, JPH::Vec3& pos, JPH::Quat& quat) {

            Common::MatrixDecomposition decomp(matrix);

            pos = JPH::Vec3(decomp.translation.x, decomp.translation.y, decomp.translation.z);
            quat = JPH::Quat(decomp.quaternion.x, decomp.quaternion.y, decomp.quaternion.z, decomp.quaternion.w);

        }

        inline void JPHPosAndRotToMatrix(const JPH::Vec3& pos, const JPH::Quat& quat, mat4& matrix) {



        }

        inline JPH::Vec3 VecToJPHVec(const vec3& vec) {

            return JPH::Vec3(vec.x, vec.y, vec.z);

        }

        inline vec3 JPHVecToVec(const JPH::Vec3& vec) {

            return vec3(vec.GetX(), vec.GetY(), vec.GetZ());

        }

        inline mat4 JPHMatToMat(const JPH::Mat44& mat) {

            mat4 matrix;
            for (int8_t i = 0; i < 4; i++) {
                auto col = mat.GetColumn4(i);
                matrix[i] = vec4(col.GetX(), col.GetY(), col.GetZ(), col.GetW());
            }

            return matrix;

        }

    }

}