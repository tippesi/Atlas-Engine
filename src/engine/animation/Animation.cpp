#include "Animation.h"

#include <algorithm>
#include <limits>

namespace Atlas {

    namespace Animation {

        vec3 Channel::SamplePosition(float time, vec3 defaultPosition) const {

            if (positions.empty())
                return defaultPosition;
            if (positions.size() == 1)
                return positions.front();

            float t;
            auto idx = FindKey(positionTimes, time, t);
            return glm::mix(positions[idx], positions[idx + 1], t);

        }

        quat Channel::SampleRotation(float time, quat defaultRotation) const {

            if (rotations.empty())
                return defaultRotation;
            if (rotations.size() == 1)
                return rotations.front();

            float t;
            auto idx = FindKey(rotationTimes, time, t);
            return glm::slerp(rotations[idx], rotations[idx + 1], t);

        }

        vec3 Channel::SampleScale(float time, vec3 defaultScale) const {

            if (scales.empty())
                return defaultScale;
            if (scales.size() == 1)
                return scales.front();

            float t;
            auto idx = FindKey(scaleTimes, time, t);
            return glm::mix(scales[idx], scales[idx + 1], t);

        }

        size_t Channel::FindKey(const std::vector<float>& times, float time, float& t) const {

            auto upper = std::upper_bound(times.begin(), times.end(), time);
            if (upper == times.begin()) {
                t = 0.0f;
                return 0;
            }
            if (upper == times.end()) {
                t = 1.0f;
                return times.size() - 2;
            }

            auto idx = size_t(upper - times.begin()) - 1;
            auto delta = times[idx + 1] - times[idx];
            t = delta > 0.0f ? (time - times[idx]) / delta : 0.0f;

            return idx;

        }

        void Animation::Sample(float time, const Mesh::Skeleton& skeleton, const std::vector<int32_t>& channelToJoint,
            std::vector<Mesh::JointPose>& pose) const {

            skeleton.GetRestPose(pose);

            for (size_t i = 0; i < channels.size() && i < channelToJoint.size(); i++) {
                auto jointIdx = channelToJoint[i];
                if (jointIdx < 0)
                    continue;

                const auto& channel = channels[i];
                const auto& joint = skeleton.joints[jointIdx];

                pose[jointIdx].position = channel.SamplePosition(time, joint.position);
                pose[jointIdx].rotation = channel.SampleRotation(time, joint.rotation);
                pose[jointIdx].scale = channel.SampleScale(time, joint.scale);
            }

        }

        float Animation::MeasureStride(const Mesh::Skeleton& skeleton, const std::string& joint) const {

            auto jointIdx = skeleton.GetJointIndex(joint);
            if (jointIdx < 0 || duration <= 0.0f)
                return 0.0f;

            std::vector<int32_t> channelToJoint;
            MapChannels(skeleton, channelToJoint);

            const int32_t samples = 128;
            std::vector<Mesh::JointPose> pose;
            std::vector<mat4> globalMatrices;
            std::vector<vec3> positions(samples);
            auto lowest = std::numeric_limits<float>::max();
            auto highest = -std::numeric_limits<float>::max();
            for (int32_t i = 0; i < samples; i++) {
                Sample(duration * float(i) / float(samples), skeleton, channelToJoint, pose);
                skeleton.GetGlobalMatrices(pose, globalMatrices);
                positions[i] = vec3(globalMatrices[jointIdx][3]);
                lowest = glm::min(lowest, positions[i].y);
                highest = glm::max(highest, positions[i].y);
            }

            // The foot stands while it is in the lowest part of its lift
            auto standing = lowest + 0.15f * (highest - lowest);
            auto distance = 0.0f;
            auto count = 0;
            for (int32_t i = 0; i < samples; i++) {
                const auto& p0 = positions[i];
                const auto& p1 = positions[(i + 1) % samples];
                if (p0.y > standing || p1.y > standing)
                    continue;
                distance += glm::length(vec2(p1.x - p0.x, p1.z - p0.z));
                count++;
            }
            if (!count)
                return 0.0f;

            auto speed = distance / (float(count) * duration / float(samples));
            return speed * duration;

        }

        void Animation::MapChannels(const Mesh::Skeleton& skeleton, std::vector<int32_t>& channelToJoint) const {

            channelToJoint.resize(channels.size());

            for (size_t i = 0; i < channels.size(); i++) {
                channelToJoint[i] = skeleton.GetJointIndex(channels[i].jointName);
            }

        }

    }

}
