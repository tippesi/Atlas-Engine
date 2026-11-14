#pragma once

#include "../../System.h"
#include "../../animation/Animation.h"
#include "../../mesh/Skeleton.h"
#include "../../resource/Resource.h"
#include "../../volume/AABB.h"

#include <vector>
#include <deque>

namespace Atlas {

    namespace Scene {

        class Scene;

        namespace Components {

            enum class AnimationBlendMode {
                // The layer replaces the pose of the joints it covers, the rotations in mesh space
                Override = 0,
                // The layer adds its motion relative to its first frame, e.g. breathing or a flinch
                Additive
            };

            class AnimationComponent;

            // An animation with its playback state and the pose sampled from it
            class AnimationClipPlayer {

            public:
                ResourceHandle<Animation::Animation> animation;

                float time = 0.0f;
                float speed = 1.0f;
                bool loop = true;
                bool playing = true;

            protected:
                void StartClip(const ResourceHandle<Animation::Animation>& animation, float blendTime, bool blend);

                void AdvanceClip(float deltaTime);

                // Fades out the blend pose, ends the blend once it is gone
                void UpdateBlend(float deltaTime);

                static float AdvanceTime(const Animation::Animation& animation, float time, float deltaTime,
                    float speed, bool loop);

                static void BlendPoses(std::vector<Mesh::JointPose>& pose, const std::vector<Mesh::JointPose>& from,
                    float weight);

                std::vector<int32_t> channelToJoint;
                std::vector<Mesh::JointPose> pose;
                const Animation::Animation* mappedAnimation = nullptr;
                const Mesh::Skeleton* mappedSkeleton = nullptr;

                std::vector<Mesh::JointPose> blendPose;
                float blendTime = 0.0f;
                float blendProgress = 0.0f;
                bool blending = false;

                friend AnimationComponent;

            };

            class AnimationLayer : public AnimationClipPlayer {

            public:
                /**
                 * Plays an animation on the layer from its start
                 * @param animation The animation to play
                 * @param blendTime The time in seconds to blend from the last pose of the layer to the new animation
                 */
                void Play(const ResourceHandle<Animation::Animation>& animation, float blendTime = 0.0f);

                /**
                 * Fades the influence of the layer to a weight
                 * @param weight The weight from 0 (no influence) to 1
                 * @param fadeTime The time in seconds of the fade, 0 changes it at once
                 */
                void FadeTo(float weight, float fadeTime = 0.0f);

                /**
                 * Restricts the layer to a joint and the joints below it
                 * @param rootJoint The name of the joint, an empty name covers the whole skeleton
                 * @param blendJoints The number of joints from the root down over which the influence rises to the full
                 * weight, e.g. along the spine, so the upper body doesn't twist against the hips
                 */
                void SetMask(const std::string& rootJoint, int32_t blendJoints = 0);

                float GetWeight() const { return weight; }

                float GetTargetWeight() const { return targetWeight; }

                const std::string& GetMaskRoot() const { return maskRoot; }

                int32_t GetMaskBlendJoints() const { return maskBlendJoints; }

                AnimationBlendMode mode = AnimationBlendMode::Override;

            private:
                float weight = 1.0f;
                float targetWeight = 1.0f;
                float fadeSpeed = 0.0f;

                std::string maskRoot;
                int32_t maskBlendJoints = 0;
                bool maskChanged = true;

                // Influence of the layer on each joint, empty for the whole skeleton
                std::vector<float> mask;
                // The first frame, additive layers add the motion relative to it
                std::vector<Mesh::JointPose> referencePose;

                friend AnimationComponent;

            };

            class AnimationIKChain {

            public:
                /**
                 * Sets the joints of the chain: the end joint is placed on the target by turning the upper and the
                 * middle joint. An end joint outside the hierarchy of the middle joint, e.g. a separate control joint
                 * of a hoof, is moved along with the tip of the middle joint
                 */
                void SetJoints(const std::string& upperJoint, const std::string& middleJoint, const std::string& endJoint);

                const std::string& GetUpperJoint() const { return upperJoint; }

                const std::string& GetMiddleJoint() const { return middleJoint; }

                const std::string& GetEndJoint() const { return endJoint; }

                /**
                 * Returns the world position of the end joint after the animation, its layers and the joint offsets,
                 * before the chain was solved. Valid after the first update with all joints found
                 */
                vec3 GetAnimatedEndPosition() const { return animatedEndPosition; }

                /**
                 * Returns the same position in the space of the mesh, which doesn't depend on the transform the entity
                 * had in the update, e.g. to measure the lift of a foot while the entity moves
                 */
                vec3 GetAnimatedEndLocalPosition() const { return animatedEndLocalPosition; }

                bool IsValid() const { return upperIdx >= 0 && middleIdx >= 0 && endIdx >= 0; }

                // World position the end joint reaches for, or with a relative target the world offset from where
                // the animation puts the end joint in the same update (no lag behind a moving entity)
                vec3 target = vec3(0.0f);
                bool relativeTarget = false;
                // Influence of the chain, 0 leaves the animation unchanged
                float weight = 0.0f;

                // Direction in mesh space the middle joint bends to when the chain is straight
                vec3 pole = vec3(0.0f, 0.0f, 1.0f);

                // World normal the end joint turns the up axis of the mesh to, e.g. a foot onto a slope
                vec3 normal = vec3(0.0f, 1.0f, 0.0f);
                float normalWeight = 0.0f;

            private:
                std::string upperJoint;
                std::string middleJoint;
                std::string endJoint;

                int32_t upperIdx = -1;
                int32_t middleIdx = -1;
                int32_t endIdx = -1;
                bool endFollowsMiddle = true;

                const Mesh::Skeleton* mappedSkeleton = nullptr;
                vec3 animatedEndPosition = vec3(0.0f);
                vec3 animatedEndLocalPosition = vec3(0.0f);
                // A separate end joint in the space of the middle joint, before the joint offsets moved them apart
                vec3 tip = vec3(0.0f);

                friend AnimationComponent;

            };

            class AnimationComponent : public AnimationClipPlayer {

            public:
                AnimationComponent() = default;

                explicit AnimationComponent(const ResourceHandle<Animation::Animation>& animation) {
                    this->animation = animation;
                }

                /**
                 * Plays an animation from its start
                 * @param animation The animation to play
                 * @param blendTime The time in seconds to blend from the current animation to the new one
                 */
                void Play(const ResourceHandle<Animation::Animation>& animation, float blendTime = 0.0f);

                /**
                 * Returns a layer which blends over the animation of the component, layers are applied in the order
                 * of their index. Missing layers up to the index are created, references to layers stay valid
                 */
                AnimationLayer& GetLayer(int32_t index);

                int32_t GetLayerCount() const;

                /**
                 * Rotates a joint after the animation and its layers are applied, e.g. to turn the head or the spine
                 * towards a target. The offsets of joints in one chain add up
                 * @param jointIdx The index of the joint in the skeleton
                 * @param rotation The rotation in the space of the mesh, the identity removes the offset
                 */
                void SetJointRotationOffset(int32_t jointIdx, quat rotation);

                void ClearJointRotationOffsets();

                /**
                 * Moves a joint after the animation and its layers are applied, e.g. lowers the hips such that the feet
                 * reach the ground with inverse kinematics
                 * @param jointIdx The index of the joint in the skeleton
                 * @param translation The translation in the space of the mesh, zero removes the offset
                 */
                void SetJointTranslationOffset(int32_t jointIdx, vec3 translation);

                void ClearJointTranslationOffsets();

                /**
                 * Returns an inverse kinematics chain, solved in order of the index after the animation, its layers
                 * and the joint offsets. Missing chains up to the index are created, references to chains stay valid
                 */
                AnimationIKChain& GetIKChain(int32_t index);

                int32_t GetIKChainCount() const;

                /**
                 * Returns the matrix of a joint in the space of the mesh, which is the identity for invalid joints
                 */
                mat4 GetJointMatrix(int32_t jointIdx) const;

                /**
                 * Returns whether the last scene update changed the pose. Animations further away from the camera
                 * update less often, see the animation settings of the mesh
                 */
                bool IsPoseUpdated() const { return poseUpdated; }

                std::deque<AnimationLayer> layers;
                std::deque<AnimationIKChain> ikChains;

                // Transposed skinning matrices of the current and last frame
                std::vector<mat3x4> jointMatrices;
                std::vector<mat3x4> lastJointMatrices;

                // Bounds of the posed mesh in its local space
                Volume::AABB aabb;

            private:
                void Update(const Mesh::Skeleton& skeleton, float deltaTime, const mat4& globalMatrix);

                void Skip(float deltaTime);

                void UpdateLayer(AnimationLayer& layer, const Mesh::Skeleton& skeleton, float deltaTime);

                void ApplyJointOffsets(const Mesh::Skeleton& skeleton);

                void PrepareIKChain(AnimationIKChain& chain, const Mesh::Skeleton& skeleton) const;

                void SolveIKChain(AnimationIKChain& chain, const Mesh::Skeleton& skeleton, const mat4& globalMatrix,
                    const mat4& inverseGlobalMatrix);

                quat GetJointRotation(int32_t jointIdx) const;

                void RotateJoint(int32_t jointIdx, const quat& rotation, const Mesh::Skeleton& skeleton);

                std::vector<mat4> globalMatrices;
                // Mesh space rotations of the layer and of the blended pose while an override layer is applied
                std::vector<quat> layerRotations;
                std::vector<quat> blendedRotations;

                // The previous animation or, when a blend is interrupted, its last pose is faded out
                ResourceHandle<Animation::Animation> blendAnimation;
                std::vector<int32_t> blendChannelToJoint;
                float blendAnimationTime = 0.0f;

                std::vector<std::pair<int32_t, quat>> rotationOffsets;
                std::vector<std::pair<int32_t, vec3>> translationOffsets;

                float skippedDeltaTime = 0.0f;
                bool poseUpdated = false;

                friend Scene;

            };

        }

    }

}
