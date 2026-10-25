#pragma once

#include "../System.h"
#include "../mesh/Skeleton.h"

#include <vector>

namespace Atlas {

    namespace Animation {

        class Channel {

        public:
            vec3 SamplePosition(float time, vec3 defaultPosition) const;

            quat SampleRotation(float time, quat defaultRotation) const;

            vec3 SampleScale(float time, vec3 defaultScale) const;

            std::string jointName;

            std::vector<float> positionTimes;
            std::vector<vec3> positions;

            std::vector<float> rotationTimes;
            std::vector<quat> rotations;

            std::vector<float> scaleTimes;
            std::vector<vec3> scales;

        private:
            size_t FindKey(const std::vector<float>& times, float time, float& t) const;

        };

        class Animation {

        public:
            Animation() = default;

            /**
             * Samples the local joint transforms of the skeleton at a point in time
             * @param time The time in seconds
             * @param skeleton The skeleton to sample the animation for
             * @param channelToJoint Maps the channels to the joints of the skeleton, see {@link MapChannels}
             * @param pose The local transform of each joint
             * @note Joints without a channel keep their rest pose
             */
            void Sample(float time, const Mesh::Skeleton& skeleton, const std::vector<int32_t>& channelToJoint,
                std::vector<Mesh::JointPose>& pose) const;

            /**
             * Maps each channel to the joint of the skeleton with the same name
             * @param skeleton
             * @param channelToJoint The joint index of each channel or -1 if the skeleton doesn't have the joint
             */
            void MapChannels(const Mesh::Skeleton& skeleton, std::vector<int32_t>& channelToJoint) const;

            std::string name;

            float duration = 0.0f;

            std::vector<Channel> channels;

        };

    }

}
