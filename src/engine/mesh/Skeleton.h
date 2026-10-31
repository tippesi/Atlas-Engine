#pragma once

#include "../System.h"
#include "../volume/AABB.h"

#include <vector>

namespace Atlas {

    namespace Mesh {

        struct JointPose {
            vec3 position = vec3(0.0f);
            quat rotation = quat(1.0f, 0.0f, 0.0f, 0.0f);
            vec3 scale = vec3(1.0f);
        };

        struct Joint {
            std::string name;
            int32_t parentIdx = -1;

            mat4 inverseBindMatrix = mat4(1.0f);

            // Local transform of the rest pose
            vec3 position = vec3(0.0f);
            quat rotation = quat(1.0f, 0.0f, 0.0f, 0.0f);
            vec3 scale = vec3(1.0f);

            // Bounds of the influenced vertices in joint space
            Volume::AABB aabb;
            bool hasVertices = false;
        };

        class Skeleton {

        public:
            int32_t GetJointIndex(const std::string& name) const;

            int32_t GetJointCount() const;

            void GetRestPose(std::vector<JointPose>& pose) const;

            void GetGlobalMatrices(const std::vector<JointPose>& pose, std::vector<mat4>& globalMatrices) const;

            /**
             * Updates the skinning matrices of the rest pose, needs to be called after the joints changed
             */
            void UpdateRestPose();

            // Joints are sorted such that parents come before their children
            std::vector<Joint> joints;

            // Transposed skinning matrices and bounds of the rest pose
            std::vector<mat3x4> restJointMatrices;
            Volume::AABB restAABB;

        };

    }

}
