#include "AnimationComponent.h"

#include "common/MathHelper.h"

#include <algorithm>
#include <cstring>
#include <glm/gtx/quaternion.hpp>

namespace Atlas {

    namespace Scene {

        namespace Components {

            void AnimationClipPlayer::StartClip(const ResourceHandle<Animation::Animation>& animation, float blendTime, bool blend) {

                blending = blend;
                this->blendTime = blendTime;
                blendProgress = 0.0f;

                this->animation = animation;
                time = 0.0f;

            }

            void AnimationClipPlayer::AdvanceClip(float deltaTime) {

                if (playing)
                    time = AdvanceTime(*animation.Get(), time, deltaTime, speed, loop);

            }

            void AnimationClipPlayer::UpdateBlend(float deltaTime) {

                // The blend also progresses while the animations are stopped, e.g. to preview a change
                blendProgress += deltaTime;
                auto weight = blendPose.size() == pose.size() ? Helper::Math::Saturate(blendProgress / blendTime) : 1.0f;
                BlendPoses(pose, blendPose, weight);
                blending = weight < 1.0f;

            }

            void AnimationLayer::Play(const ResourceHandle<Animation::Animation>& animation, float blendTime) {

                auto canBlend = blendTime > 0.0f && !pose.empty() && weight > 0.0f;
                if (canBlend)
                    blendPose = pose;

                StartClip(animation, blendTime, canBlend);

            }

            void AnimationLayer::FadeTo(float weight, float fadeTime) {

                targetWeight = Helper::Math::Saturate(weight);
                fadeSpeed = fadeTime > 0.0f ? glm::abs(targetWeight - this->weight) / fadeTime : 0.0f;
                if (fadeSpeed <= 0.0f)
                    this->weight = targetWeight;

            }

            void AnimationLayer::SetMask(const std::string& rootJoint, int32_t blendJoints) {

                maskRoot = rootJoint;
                maskBlendJoints = glm::max(blendJoints, 0);
                maskChanged = true;

            }

            void AnimationIKChain::SetJoints(const std::string& upperJoint, const std::string& middleJoint,
                const std::string& endJoint) {

                this->upperJoint = upperJoint;
                this->middleJoint = middleJoint;
                this->endJoint = endJoint;
                mappedSkeleton = nullptr;

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

                StartClip(animation, blendTime, canBlend);

            }

            AnimationLayer& AnimationComponent::GetLayer(int32_t index) {

                AE_ASSERT(index >= 0 && "Layer index must not be negative");
                index = glm::max(index, 0);

                while (int32_t(layers.size()) <= index)
                    layers.emplace_back();

                return layers[index];

            }

            int32_t AnimationComponent::GetLayerCount() const {

                return int32_t(layers.size());

            }

            void AnimationComponent::SetJointRotationOffset(int32_t jointIdx, quat rotation) {

                auto identity = glm::abs(glm::dot(rotation, quat(1.0f, 0.0f, 0.0f, 0.0f))) > 0.999999f;
                auto offset = std::find_if(rotationOffsets.begin(), rotationOffsets.end(),
                    [&](const auto& offset) { return offset.first == jointIdx; });

                if (offset != rotationOffsets.end()) {
                    if (identity)
                        rotationOffsets.erase(offset);
                    else
                        offset->second = rotation;
                }
                else if (!identity) {
                    rotationOffsets.emplace_back(jointIdx, rotation);
                }

            }

            void AnimationComponent::ClearJointRotationOffsets() {

                rotationOffsets.clear();

            }

            void AnimationComponent::SetJointTranslationOffset(int32_t jointIdx, vec3 translation) {

                auto offset = std::find_if(translationOffsets.begin(), translationOffsets.end(),
                    [&](const auto& offset) { return offset.first == jointIdx; });
                auto zero = glm::dot(translation, translation) <= 0.0f;

                if (offset != translationOffsets.end()) {
                    if (zero)
                        translationOffsets.erase(offset);
                    else
                        offset->second = translation;
                }
                else if (!zero) {
                    translationOffsets.emplace_back(jointIdx, translation);
                }

            }

            void AnimationComponent::ClearJointTranslationOffsets() {

                translationOffsets.clear();

            }

            AnimationIKChain& AnimationComponent::GetIKChain(int32_t index) {

                AE_ASSERT(index >= 0 && "Chain index must not be negative");
                index = glm::max(index, 0);

                while (int32_t(ikChains.size()) <= index)
                    ikChains.emplace_back();

                return ikChains[index];

            }

            int32_t AnimationComponent::GetIKChainCount() const {

                return int32_t(ikChains.size());

            }

            mat4 AnimationComponent::GetJointMatrix(int32_t jointIdx) const {

                if (jointIdx < 0 || jointIdx >= int32_t(globalMatrices.size()))
                    return mat4(1.0f);

                return globalMatrices[jointIdx];

            }

            void AnimationComponent::Update(const Mesh::Skeleton& skeleton, float deltaTime, const mat4& globalMatrix) {

                deltaTime += skippedDeltaTime;
                skippedDeltaTime = 0.0f;

                if (animation.IsLoaded()) {
                    AdvanceClip(deltaTime);

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
                            blendAnimationTime = AdvanceTime(*blendAnimation.Get(), blendAnimationTime, deltaTime, speed, loop);
                        blendAnimation->Sample(blendAnimationTime, skeleton, blendChannelToJoint, blendPose);
                    }

                    UpdateBlend(deltaTime);
                    if (!blending)
                        blendAnimation.Reset();
                }

                for (auto& layer : layers)
                    UpdateLayer(layer, skeleton, deltaTime);

                skeleton.GetGlobalMatrices(pose, globalMatrices);

                for (auto& chain : ikChains)
                    PrepareIKChain(chain, skeleton);

                if (!rotationOffsets.empty() || !translationOffsets.empty())
                    ApplyJointOffsets(skeleton);

                if (!ikChains.empty()) {
                    auto inverseGlobalMatrix = glm::inverse(globalMatrix);
                    for (auto& chain : ikChains)
                        SolveIKChain(chain, skeleton, globalMatrix, inverseGlobalMatrix);
                }

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

                poseUpdated = !hasHistory || std::memcmp(jointMatrices.data(), lastJointMatrices.data(),
                    jointMatrices.size() * sizeof(mat3x4)) != 0;

            }

            void AnimationComponent::Skip(float deltaTime) {

                skippedDeltaTime += deltaTime;

                // The pose stands still until the next update and has no motion
                if (poseUpdated)
                    lastJointMatrices = jointMatrices;
                poseUpdated = false;

            }

            void AnimationComponent::UpdateLayer(AnimationLayer& layer, const Mesh::Skeleton& skeleton, float deltaTime) {

                if (layer.weight != layer.targetWeight) {
                    auto step = layer.fadeSpeed * deltaTime;
                    layer.weight = layer.weight < layer.targetWeight ? glm::min(layer.weight + step, layer.targetWeight) :
                        glm::max(layer.weight - step, layer.targetWeight);
                }

                // A layer without influence stands still until it fades in again
                if (!layer.animation.IsLoaded() || (layer.weight <= 0.0f && layer.targetWeight <= 0.0f))
                    return;

                const auto& animation = *layer.animation.Get();
                layer.AdvanceClip(deltaTime);

                if (layer.mappedAnimation != &animation || layer.mappedSkeleton != &skeleton) {
                    layer.maskChanged |= layer.mappedSkeleton != &skeleton;
                    animation.MapChannels(skeleton, layer.channelToJoint);
                    animation.Sample(0.0f, skeleton, layer.channelToJoint, layer.referencePose);
                    layer.mappedAnimation = &animation;
                    layer.mappedSkeleton = &skeleton;
                }

                if (layer.maskChanged) {
                    layer.mask.clear();
                    if (!layer.maskRoot.empty()) {
                        auto jointCount = skeleton.joints.size();
                        // A mask with an unknown root covers nothing instead of the whole skeleton
                        layer.mask.assign(jointCount, 0.0f);

                        std::vector<int32_t> depth(jointCount, -1);
                        auto rootIdx = skeleton.GetJointIndex(layer.maskRoot);
                        for (int32_t i = glm::max(rootIdx, 0); rootIdx >= 0 && i < int32_t(jointCount); i++) {
                            auto parentIdx = skeleton.joints[i].parentIdx;
                            if (i == rootIdx)
                                depth[i] = 0;
                            else if (parentIdx >= 0 && depth[parentIdx] >= 0)
                                depth[i] = depth[parentIdx] + 1;

                            if (depth[i] >= 0)
                                layer.mask[i] = glm::min(1.0f, float(depth[i] + 1) / float(layer.maskBlendJoints + 1));
                        }
                    }
                    layer.maskChanged = false;
                }

                animation.Sample(layer.time, skeleton, layer.channelToJoint, layer.pose);

                if (layer.blending)
                    layer.UpdateBlend(deltaTime);

                if (layer.pose.size() != pose.size())
                    return;

                // Overrides blend the rotations in mesh space: a layer covering the upper body keeps the orientation
                // its clip gives the torso and the head, whatever the joints below the mask do
                layerRotations.resize(pose.size());
                blendedRotations.resize(pose.size());

                for (size_t i = 0; i < pose.size(); i++) {
                    auto weight = layer.weight * (layer.mask.empty() ? 1.0f : layer.mask[i]);

                    auto& jointPose = pose[i];
                    const auto& layerPose = layer.pose[i];
                    if (layer.mode == AnimationBlendMode::Override) {
                        // Parents come before their children
                        auto parentIdx = skeleton.joints[i].parentIdx;
                        auto layerParent = parentIdx >= 0 ? layerRotations[parentIdx] : quat(1.0f, 0.0f, 0.0f, 0.0f);
                        auto parent = parentIdx >= 0 ? blendedRotations[parentIdx] : quat(1.0f, 0.0f, 0.0f, 0.0f);
                        layerRotations[i] = layerParent * layerPose.rotation;

                        if (weight > 0.0f) {
                            auto rotation = glm::slerp(parent * jointPose.rotation, layerRotations[i], weight);
                            jointPose.position = glm::mix(jointPose.position, layerPose.position, weight);
                            jointPose.rotation = glm::normalize(glm::inverse(parent) * rotation);
                            jointPose.scale = glm::mix(jointPose.scale, layerPose.scale, weight);
                        }
                        blendedRotations[i] = parent * jointPose.rotation;
                    }
                    else if (weight > 0.0f) {
                        const auto& reference = layer.referencePose[i];
                        auto rotation = glm::inverse(reference.rotation) * layerPose.rotation;
                        auto scale = glm::mix(vec3(1.0f), layerPose.scale / glm::max(reference.scale, vec3(1e-6f)), weight);
                        jointPose.position += (layerPose.position - reference.position) * weight;
                        jointPose.rotation = jointPose.rotation * glm::slerp(quat(1.0f, 0.0f, 0.0f, 0.0f), rotation, weight);
                        jointPose.scale *= scale;
                    }
                }

            }

            void AnimationComponent::ApplyJointOffsets(const Mesh::Skeleton& skeleton) {

                // The rotation of a joint in the space of its parent is turned by the offset in mesh space, the
                // rotations of the parents are taken from the pose before any offset
                for (const auto& [jointIdx, rotation] : rotationOffsets) {
                    if (jointIdx < 0 || jointIdx >= int32_t(pose.size()))
                        continue;

                    auto parentRotation = GetJointRotation(skeleton.joints[jointIdx].parentIdx);
                    pose[jointIdx].rotation = glm::inverse(parentRotation) * rotation * parentRotation * pose[jointIdx].rotation;
                }

                // Translations in mesh space go through the scale of the parents as well
                for (const auto& [jointIdx, translation] : translationOffsets) {
                    if (jointIdx < 0 || jointIdx >= int32_t(pose.size()))
                        continue;

                    auto parentIdx = skeleton.joints[jointIdx].parentIdx;
                    auto parentMatrix = parentIdx >= 0 ? mat3(globalMatrices[parentIdx]) : mat3(1.0f);
                    pose[jointIdx].position += glm::inverse(parentMatrix) * translation;
                }

                skeleton.GetGlobalMatrices(pose, globalMatrices);

            }

            void AnimationComponent::PrepareIKChain(AnimationIKChain& chain, const Mesh::Skeleton& skeleton) const {

                if (chain.mappedSkeleton != &skeleton) {
                    chain.upperIdx = skeleton.GetJointIndex(chain.upperJoint);
                    chain.middleIdx = skeleton.GetJointIndex(chain.middleJoint);
                    chain.endIdx = skeleton.GetJointIndex(chain.endJoint);
                    chain.mappedSkeleton = &skeleton;

                    chain.endFollowsMiddle = false;
                    for (auto idx = chain.endIdx; idx >= 0 && !chain.endFollowsMiddle; idx = skeleton.joints[idx].parentIdx)
                        chain.endFollowsMiddle = idx == chain.middleIdx;
                }

                if (!chain.IsValid() || chain.endFollowsMiddle)
                    return;

                // The end joint is where the animation puts the tip of the middle bone, an offset of the body
                // mustn't change the length of the bone
                auto end = vec4(vec3(globalMatrices[chain.endIdx][3]), 1.0f);
                chain.tip = vec3(glm::inverse(globalMatrices[chain.middleIdx]) * end);

            }

            void AnimationComponent::SolveIKChain(AnimationIKChain& chain, const Mesh::Skeleton& skeleton,
                const mat4& globalMatrix, const mat4& inverseGlobalMatrix) {

                if (!chain.IsValid())
                    return;

                auto a = vec3(globalMatrices[chain.upperIdx][3]);
                auto b = vec3(globalMatrices[chain.middleIdx][3]);
                auto c = chain.endFollowsMiddle ? vec3(globalMatrices[chain.endIdx][3]) :
                    vec3(globalMatrices[chain.middleIdx] * vec4(chain.tip, 1.0f));
                chain.animatedEndLocalPosition = vec3(globalMatrices[chain.endIdx][3]);
                chain.animatedEndPosition = vec3(globalMatrix * vec4(chain.animatedEndLocalPosition, 1.0f));

                if (chain.weight <= 0.0f)
                    return;

                auto lengthAB = glm::distance(a, b);
                auto lengthBC = glm::distance(b, c);
                if (lengthAB <= 1e-6f || lengthBC <= 1e-6f || glm::distance(a, c) <= 1e-6f)
                    return;

                auto target = chain.relativeTarget ? c + mat3(inverseGlobalMatrix) * chain.target :
                    vec3(inverseGlobalMatrix * vec4(chain.target, 1.0f));
                target = glm::mix(c, target, glm::min(chain.weight, 1.0f));
                auto lengthAT = glm::clamp(glm::distance(a, target), 1e-5f, (lengthAB + lengthBC) * 0.9999f);

                // The triangle of the two bones: the angles at the upper and the middle joint before and after, then
                // the turn of the whole chain onto the target
                auto angle = [](vec3 v0, vec3 v1) {
                    return glm::acos(glm::clamp(glm::dot(glm::normalize(v0), glm::normalize(v1)), -1.0f, 1.0f));
                    };
                auto lawOfCosines = [](float opposite, float side0, float side1) {
                    return glm::acos(glm::clamp((side0 * side0 + side1 * side1 - opposite * opposite) / (2.0f * side0 * side1), -1.0f, 1.0f));
                    };

                auto upperAngle = angle(c - a, b - a);
                auto middleAngle = angle(a - b, c - b);
                auto solvedUpperAngle = lawOfCosines(lengthBC, lengthAB, lengthAT);
                auto solvedMiddleAngle = lawOfCosines(lengthAT, lengthAB, lengthBC);

                // A straight chain bends towards the pole
                auto bendAxis = glm::cross(c - a, b - a);
                if (glm::length(bendAxis) <= 1e-4f * lengthAB * lengthAB)
                    bendAxis = glm::cross(c - a, chain.pole);
                if (glm::length(bendAxis) <= 1e-12f)
                    return;
                bendAxis = glm::normalize(bendAxis);

                auto targetAxis = glm::cross(c - a, target - a);
                auto targetTurn = glm::length(targetAxis) > 1e-12f ?
                    glm::angleAxis(angle(c - a, target - a), glm::normalize(targetAxis)) : quat(1.0f, 0.0f, 0.0f, 0.0f);
                auto upperTurn = targetTurn * glm::angleAxis(solvedUpperAngle - upperAngle, bendAxis);
                auto middleTurn = glm::angleAxis(solvedMiddleAngle - middleAngle, bendAxis);

                // Both turns are rotations in mesh space relative to the joints before the chain was solved
                auto upperRotation = GetJointRotation(chain.upperIdx);
                auto middleRotation = GetJointRotation(chain.middleIdx);

                auto& upperPose = pose[chain.upperIdx];
                auto& middlePose = pose[chain.middleIdx];
                upperPose.rotation = upperPose.rotation * glm::inverse(upperRotation) * upperTurn * upperRotation;
                middlePose.rotation = middlePose.rotation * glm::inverse(middleRotation) * middleTurn * middleRotation;
                skeleton.UpdateGlobalMatrices(pose, globalMatrices, { chain.upperIdx, chain.middleIdx });

                if (!chain.endFollowsMiddle) {
                    // The end joint goes to the new tip of the middle bone and turns like the middle joint
                    auto tip = globalMatrices[chain.middleIdx] * vec4(chain.tip, 1.0f);
                    auto endParentIdx = skeleton.joints[chain.endIdx].parentIdx;
                    auto endParentMatrix = endParentIdx >= 0 ? globalMatrices[endParentIdx] : mat4(1.0f);
                    pose[chain.endIdx].position = vec3(glm::inverse(endParentMatrix) * tip);
                    skeleton.UpdateGlobalMatrices(pose, globalMatrices, { chain.endIdx });

                    RotateJoint(chain.endIdx, GetJointRotation(chain.middleIdx) * glm::inverse(middleRotation), skeleton);
                }

                if (chain.normalWeight > 0.0f && glm::dot(chain.normal, chain.normal) > 0.0f) {
                    auto normal = glm::normalize(mat3(inverseGlobalMatrix) * chain.normal);
                    auto turn = glm::rotation(vec3(0.0f, 1.0f, 0.0f), normal);
                    RotateJoint(chain.endIdx, glm::slerp(quat(1.0f, 0.0f, 0.0f, 0.0f), turn, glm::min(chain.normalWeight, 1.0f)), skeleton);
                }

            }

            quat AnimationComponent::GetJointRotation(int32_t jointIdx) const {

                if (jointIdx < 0 || jointIdx >= int32_t(globalMatrices.size()))
                    return quat(1.0f, 0.0f, 0.0f, 0.0f);

                auto matrix = mat3(globalMatrices[jointIdx]);
                return glm::normalize(glm::quat_cast(mat3(glm::normalize(matrix[0]), glm::normalize(matrix[1]),
                    glm::normalize(matrix[2]))));

            }

            void AnimationComponent::RotateJoint(int32_t jointIdx, const quat& rotation, const Mesh::Skeleton& skeleton) {

                // Turns the joint by a rotation in mesh space
                auto jointRotation = GetJointRotation(jointIdx);
                pose[jointIdx].rotation = pose[jointIdx].rotation * glm::inverse(jointRotation) * rotation * jointRotation;
                skeleton.UpdateGlobalMatrices(pose, globalMatrices, { jointIdx });

            }

            float AnimationClipPlayer::AdvanceTime(const Animation::Animation& animation, float time, float deltaTime,
                float speed, bool loop) {

                auto duration = animation.duration;
                time += deltaTime * speed;

                if (loop && duration > 0.0f)
                    return time - glm::floor(time / duration) * duration;

                return glm::clamp(time, 0.0f, duration);

            }

            void AnimationClipPlayer::BlendPoses(std::vector<Mesh::JointPose>& pose, const std::vector<Mesh::JointPose>& from,
                float weight) {

                for (size_t i = 0; i < pose.size() && i < from.size() && weight < 1.0f; i++) {
                    pose[i].position = glm::mix(from[i].position, pose[i].position, weight);
                    pose[i].rotation = glm::slerp(from[i].rotation, pose[i].rotation, weight);
                    pose[i].scale = glm::mix(from[i].scale, pose[i].scale, weight);
                }

            }

        }

    }

}
