#include "Skeleton.h"

namespace Atlas {

    namespace Mesh {

        int32_t Skeleton::GetJointIndex(const std::string& name) const {

            for (size_t i = 0; i < joints.size(); i++) {
                if (joints[i].name == name)
                    return int32_t(i);
            }

            return -1;

        }

        int32_t Skeleton::GetJointCount() const {

            return int32_t(joints.size());

        }

        void Skeleton::GetRestPose(std::vector<JointPose>& pose) const {

            pose.resize(joints.size());

            for (size_t i = 0; i < joints.size(); i++) {
                const auto& joint = joints[i];
                pose[i] = JointPose { .position = joint.position, .rotation = joint.rotation, .scale = joint.scale };
            }

        }

        void Skeleton::GetGlobalMatrices(const std::vector<JointPose>& pose, std::vector<mat4>& globalMatrices) const {

            globalMatrices.resize(joints.size());

            for (size_t i = 0; i < joints.size(); i++) {
                auto localMatrix = glm::translate(pose[i].position) * glm::mat4_cast(pose[i].rotation) *
                    glm::scale(pose[i].scale);

                auto parentIdx = joints[i].parentIdx;
                globalMatrices[i] = parentIdx >= 0 ? globalMatrices[parentIdx] * localMatrix : localMatrix;
            }

        }

        void Skeleton::UpdateRestPose() {

            std::vector<JointPose> pose;
            std::vector<mat4> globalMatrices;
            GetRestPose(pose);
            GetGlobalMatrices(pose, globalMatrices);

            restJointMatrices.resize(joints.size());

            bool hasBounds = false;
            for (size_t i = 0; i < joints.size(); i++) {
                const auto& joint = joints[i];
                restJointMatrices[i] = mat3x4(glm::transpose(globalMatrices[i] * joint.inverseBindMatrix));

                if (!joint.hasVertices)
                    continue;

                auto jointAABB = joint.aabb.Transform(globalMatrices[i]);
                if (hasBounds)
                    restAABB.Grow(jointAABB);
                else
                    restAABB = jointAABB;
                hasBounds = true;
            }

        }

    }

}
