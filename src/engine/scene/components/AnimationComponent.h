#pragma once

#include "../../System.h"
#include "../../animation/Animation.h"
#include "../../mesh/Skeleton.h"
#include "../../resource/Resource.h"
#include "../../volume/AABB.h"

#include <vector>

namespace Atlas {

    namespace Scene {

        class Scene;

        namespace Components {

            class AnimationComponent {

            public:
                AnimationComponent() = default;
                explicit AnimationComponent(const ResourceHandle<Animation::Animation>& animation) :
                    animation(animation) {}

                /**
                 * Plays an animation from its start
                 * @param animation The animation to play
                 * @param blendTime The time in seconds to blend from the current animation to the new one
                 */
                void Play(const ResourceHandle<Animation::Animation>& animation, float blendTime = 0.0f);

                /**
                 * Returns the matrix of a joint in the space of the mesh, which is the identity for invalid joints
                 */
                mat4 GetJointMatrix(int32_t jointIdx) const;

                ResourceHandle<Animation::Animation> animation;

                float time = 0.0f;
                float speed = 1.0f;

                bool loop = true;
                bool playing = true;

                // Transposed skinning matrices of the current and last frame
                std::vector<mat3x4> jointMatrices;
                std::vector<mat3x4> lastJointMatrices;

                // Bounds of the posed mesh in its local space
                Volume::AABB aabb;

            private:
                void Update(const Mesh::Skeleton& skeleton, float deltaTime);

                float AdvanceTime(const Animation::Animation& animation, float time, float deltaTime) const;

                std::vector<int32_t> channelToJoint;
                std::vector<Mesh::JointPose> pose;
                std::vector<mat4> globalMatrices;

                const Animation::Animation* mappedAnimation = nullptr;
                const Mesh::Skeleton* mappedSkeleton = nullptr;

                // The previous animation or, when a blend is interrupted, its last pose is faded out
                ResourceHandle<Animation::Animation> blendAnimation;
                std::vector<int32_t> blendChannelToJoint;
                std::vector<Mesh::JointPose> blendPose;

                float blendAnimationTime = 0.0f;
                float blendTime = 0.0f;
                float blendProgress = 0.0f;
                bool blending = false;

                friend Scene;

            };

        }

    }

}
