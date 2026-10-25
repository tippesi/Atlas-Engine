#include "AnimationComponent.h"

namespace Atlas {

    namespace Scene {

        namespace Components {

            mat4 AnimationComponent::GetJointMatrix(int32_t jointIdx) const {

                if (jointIdx < 0 || jointIdx >= int32_t(globalMatrices.size()))
                    return mat4(1.0f);

                return globalMatrices[jointIdx];

            }

            void AnimationComponent::Play(const ResourceHandle<Animation::Animation>& animation, float blendTime) {

                auto canBlend = blendTime > 0.0f && !pose.empty();
                auto blendCurrentAnimation = canBlend && !blending && this->animation.IsLoaded() &&
                    mappedAnimation == this->animation.Get().get();

                if (blendCurrentAnimation) {
                    blendAnimation = this->animation;
                    blendChannelToJoint = channelToJoint;
                    blendAnimationTime = time;
                }
                else {
                    // Fades out the last pose, e.g. the one of an interrupted blend
                    blendPose = pose;
                    blendAnimation.Reset();
                }

                blending = canBlend;
                this->blendTime = blendTime;
                blendProgress = 0.0f;

                this->animation = animation;
                time = 0.0f;

            }

            void AnimationComponent::Update(const Mesh::Skeleton& skeleton, float deltaTime) {

                if (animation.IsLoaded()) {
                    if (playing)
                        time = AdvanceTime(*animation.Get(), time, deltaTime);

                    if (mappedAnimation != animation.Get().get() || mappedSkeleton != &skeleton) {
                        // A new skeleton invalidates the mapping of the faded out animation as well
                        if (mappedSkeleton != &skeleton && blendAnimation.IsLoaded())
                            blendAnimation->MapChannels(skeleton, blendChannelToJoint);
                        animation->MapChannels(skeleton, channelToJoint);
                        mappedAnimation = animation.Get().get();
                        mappedSkeleton = &skeleton;
                    }

                    animation->Sample(time, skeleton, channelToJoint, pose);
                }
                else {
                    skeleton.GetRestPose(pose);
                }

                if (blending) {
                    if (blendAnimation.IsLoaded()) {
                        if (playing)
                            blendAnimationTime = AdvanceTime(*blendAnimation.Get(), blendAnimationTime, deltaTime);
                        blendAnimation->Sample(blendAnimationTime, skeleton, blendChannelToJoint, blendPose);
                    }

                    // The blend also progresses while the animations are stopped, e.g. to preview a change
                    blendProgress += deltaTime;
                    auto weight = blendPose.size() == pose.size() ? glm::clamp(blendProgress / blendTime, 0.0f, 1.0f) : 1.0f;
                    for (size_t i = 0; i < pose.size() && weight < 1.0f; i++) {
                        pose[i].position = glm::mix(blendPose[i].position, pose[i].position, weight);
                        pose[i].rotation = glm::slerp(blendPose[i].rotation, pose[i].rotation, weight);
                        pose[i].scale = glm::mix(blendPose[i].scale, pose[i].scale, weight);
                    }

                    if (weight >= 1.0f) {
                        blending = false;
                        blendAnimation.Reset();
                    }
                }

                skeleton.GetGlobalMatrices(pose, globalMatrices);

                auto hasHistory = jointMatrices.size() == skeleton.joints.size();
                std::swap(jointMatrices, lastJointMatrices);
                jointMatrices.resize(skeleton.joints.size());

                auto hasBounds = false;
                for (size_t i = 0; i < skeleton.joints.size(); i++) {
                    const auto& joint = skeleton.joints[i];
                    jointMatrices[i] = mat3x4(glm::transpose(globalMatrices[i] * joint.inverseBindMatrix));

                    if (!joint.hasVertices)
                        continue;

                    auto jointAABB = joint.aabb.Transform(globalMatrices[i]);
                    if (hasBounds)
                        aabb.Grow(jointAABB);
                    else
                        aabb = jointAABB;
                    hasBounds = true;
                }

                if (!hasHistory)
                    lastJointMatrices = jointMatrices;

            }

            float AnimationComponent::AdvanceTime(const Animation::Animation& animation, float time, float deltaTime) const {

                auto duration = animation.duration;
                time += deltaTime * speed;

                if (loop && duration > 0.0f)
                    return time - glm::floor(time / duration) * duration;

                return glm::clamp(time, 0.0f, duration);

            }

        }

    }

}
